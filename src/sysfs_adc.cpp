#include "sysfs_adc.h"

#include <math.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <functional>

#include <dirent.h>
#include <fnmatch.h>
#include <unistd.h>

#include <wblib/utils.h>

namespace
{
    bool TryOpen(const std::vector<std::string>& fnames, std::ifstream& file)
    {
        for (auto& fname : fnames) {
            file.open(fname);
            if (file.is_open()) {
                return true;
            }
            file.clear();
        }
        return false;
    }

    std::string FindBestScale(const std::vector<std::string>& scales, double desiredScale)
    {
        std::string bestScaleStr;
        double bestScaleDouble = 0;

        for (auto& scaleStr : scales) {
            double val;
            try {
                val = stod(scaleStr);
            } catch (std::invalid_argument e) {
                continue;
            }
            // best scale is either maximum scale or the one closest to user request
            if (((desiredScale > 0) && (fabs(val - desiredScale) <= fabs(bestScaleDouble - desiredScale)))      // user request
                ||
                ((desiredScale <= 0) && (val >= bestScaleDouble))      // maximum scale
                )
            {
                bestScaleDouble = val;
                bestScaleStr = scaleStr;
            }
        }
        return bestScaleStr;
    }

    template<class T> void OpenWithException(T& f, const std::string& fileName)
    {
        f.open(fileName);
        if (!f.is_open()) {
            throw std::runtime_error("Can't open file:" + fileName);
        }
    }

    void WriteToFile(const std::string& fileName, const std::string& value)
    {
        std::ofstream f;
        OpenWithException(f, fileName);
        f << value;
    }

    std::string IterateDir(const std::string& dirName, const std::string& pattern, std::function<bool (const std::string& )> fn)
    {
        DIR* dir = opendir(dirName.c_str());

        if(dir == NULL) {
            throw std::runtime_error("Can't open directory: " + dirName);
        }

        std::unique_ptr<DIR, decltype(&closedir)> dirPtr(dir, closedir);
        dirent* ent;
        while ((ent = readdir (dirPtr.get())) != NULL) {
            if (!std::strstr(ent->d_name, pattern.c_str()))
                continue;
            std::string d(dirName + "/" + std::string(ent->d_name));
            if(fn(d)){
                return d;
            }
        }
        return std::string();
    }

    std::string FindSysfsIIODir2(const std::string& sysFsPrefix, const std::string& matchIIO)
    {
        if (matchIIO.empty()) {
            return sysFsPrefix + "/bus/iio/devices/iio:device0";
        }

        std::string pattern = "*" + matchIIO + "*";
        auto fn = [&](const std::string& d)
            {
                char buf[512];
                int len;
                if ((len = readlink(d.c_str(), buf, 512)) < 0)
                    return false;
                buf[len] = 0;

                return (fnmatch(pattern.c_str(), buf, 0) == 0);
            };

        std::string iioDevName = IterateDir(sysFsPrefix + "/bus/iio/devices", "iio:device", fn);

        if (iioDevName.empty()) {
            throw std::runtime_error("Can't fild matching sysfs IIO: " + matchIIO);
        }

        return iioDevName;
    }


    std::string FindSysfsIIODir(const std::string& sysFsPrefix, const std::string& matchIIO)
    {
        if (matchIIO.empty()) {
            return sysFsPrefix + "/bus/iio/devices/iio:device0";
        }

        std::string iioDevDir = sysFsPrefix + "/bus/iio/devices";

        DIR* dir = opendir(iioDevDir.c_str());
        if(dir == NULL) {
            throw std::runtime_error("Can't open directory: " + iioDevDir);
        }

        std::string iioDevName;
        std::string pattern = "*" + matchIIO + "*";
        dirent* ent;
        while ((ent = readdir (dir)) != NULL) {
            if (!std::strstr(ent->d_name, "iio:device"))
                continue;
            std::string d = iioDevDir + "/" + std::string(ent->d_name);
            char buf[512];
            int len;
            if ((len = readlink(d.c_str(), buf, 512)) < 0)
                continue;
            buf[len] = 0;

            if (fnmatch(pattern.c_str(), buf, 0) == 0) {
                iioDevName = ent->d_name;
                break;
            }
        }
        closedir(dir);

        if (iioDevName.empty()) {
            throw std::runtime_error("Can't fild matching sysfs IIO: " + matchIIO);
        }

        return iioDevDir + "/" + iioDevName;
    }
}

TChannelReader::TChannelReader(double defaultIIOScale, 
                               uint32_t maxADCvalue, 
                               const TChannelReader::TSettings& cfg, 
                               uint32_t delayBetweenMeasurementsmS,
                               WBMQTT::TLogger& debugLogger,
                               const std::string& sysFsPrefix): 
    Cfg(cfg),
    MeasuredV(0.0),
    IIOScale(defaultIIOScale),
    MaxADCValue(maxADCvalue),
    DelayBetweenMeasurementsmS(delayBetweenMeasurementsmS),
    AverageCounter(cfg.AveragingWindow),
    DebugLogger(debugLogger)
{
    SysfsIIODir = FindSysfsIIODir(sysFsPrefix, cfg.MatchIIO);
    SelectScale();
    OpenWithException(AdcValStream, SysfsIIODir + "/in_" + Cfg.ChannelNumber + "_raw");
}

std::string TChannelReader::GetValue() const
{
    std::ostringstream out;
    out << std::setprecision(Cfg.DecimalPlaces) << std::fixed << MeasuredV;
    return out.str();
}

void TChannelReader::Measure()
{
    MeasuredV = std::nan("");

    for (uint32_t i = 0; i < Cfg.ReadingsNumber; ++i) {
        uint32_t adcMeasurement = ReadFromADC();
        DebugLogger.Log() << Cfg.ChannelNumber << " = " << adcMeasurement;
        AverageCounter.AddValue(adcMeasurement);
        std::this_thread::sleep_for(std::chrono::milliseconds(DelayBetweenMeasurementsmS));
    }

    if(!AverageCounter.IsReady()) {
        DebugLogger.Log() << Cfg.ChannelNumber << " average is not ready";
        return;
    }

    uint32_t value = AverageCounter.Average();
    if (value > MaxADCValue) {
        DebugLogger.Log() << Cfg.ChannelNumber << " average (" << value <<") is bigger than maximum (" << MaxADCValue <<")";
        return;
    }

    double v = IIOScale * value;
    if (v > Cfg.MaxScaledVoltage) {
        DebugLogger.Log() << Cfg.ChannelNumber << " scaled value (" << v <<") is bigger than maximum (" << Cfg.MaxScaledVoltage <<")";
        return;
    }

    MeasuredV = v * Cfg.VoltageMultiplier / 1000.0;   //got mV let's divide it by 1000 to obtain V
}

uint32_t TChannelReader::ReadFromADC()
{
    uint32_t val;
    AdcValStream.seekg(0);
    AdcValStream >> val;
    return val;
}

void TChannelReader::SelectScale()
{
    std::string scalePrefix = SysfsIIODir + "/in_" + Cfg.ChannelNumber + "_scale";

    std::ifstream scaleFile;
    TryOpen({ scalePrefix + "_available", 
            SysfsIIODir + "/in_voltage_scale_available",
            SysfsIIODir + "/scale_available"
            }, scaleFile);

    if (scaleFile.is_open()) {
        auto contents = std::string((std::istreambuf_iterator<char>(scaleFile)), std::istreambuf_iterator<char>());
        DebugLogger.Log() << "Available scales: " << contents;

        std::string bestScaleStr = FindBestScale(WBMQTT::StringSplit(contents, " "), Cfg.Scale);

        if(!bestScaleStr.empty()) {
            IIOScale = std::stod(bestScaleStr);
            WriteToFile(scalePrefix, bestScaleStr);
            DebugLogger.Log() << scalePrefix << " is set to " << bestScaleStr;
            return;
        }
    }

    // scale_available file is not present read the current scale(in_voltageX_scale) from sysfs or from group scale(in_voltage_scale)
    TryOpen({ scalePrefix, SysfsIIODir + "/in_voltage_scale" }, scaleFile);
    if (scaleFile.is_open()) {
        scaleFile >> IIOScale;
    }
    DebugLogger.Log() << scalePrefix << " = " << IIOScale;
}