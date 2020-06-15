#include "sysfs_adc.h"

#include <fnmatch.h>
#include <iomanip>
#include <math.h>
#include <unistd.h>

#include <wblib/utils.h>

#include "file_utils.h"

TChannelReader::TChannelReader(double                           defaultIIOScale,
                               uint32_t                         maxADCvalue,
                               const TChannelReader::TSettings& cfg,
                               uint32_t                         delayBetweenMeasurementsmS,
                               WBMQTT::TLogger&                 debugLogger,
                               WBMQTT::TLogger&                 infoLogger,
                               const std::string&               sysfsIIODir)
    : Cfg(cfg), SysfsIIODir(sysfsIIODir), IIOScale(defaultIIOScale), MaxADCValue(maxADCvalue),
      DelayBetweenMeasurementsmS(delayBetweenMeasurementsmS), AverageCounter(cfg.AveragingWindow),
      DebugLogger(debugLogger)
{
    SelectScale(infoLogger);
}

std::string TChannelReader::GetValue() const
{
    return MeasuredV;
}

void TChannelReader::Measure()
{
    for (uint32_t i = 0; i < Cfg.ReadingsNumber; ++i) {
        int32_t adcMeasurement = ReadFromADC();
        DebugLogger.Log() << Cfg.ChannelNumber << " = " << adcMeasurement;
        AverageCounter.AddValue(adcMeasurement);
        std::this_thread::sleep_for(std::chrono::milliseconds(DelayBetweenMeasurementsmS));
    }

    if (!AverageCounter.IsReady()) {
        DebugLogger.Log() << Cfg.ChannelNumber << " average is not ready";
        return;
    }

    uint32_t value = AverageCounter.GetAverage();
    if (value > MaxADCValue) {
        throw std::runtime_error(Cfg.ChannelNumber + " average (" + std::to_string(value) +
                                 ") is bigger than maximum (" + std::to_string(MaxADCValue) + ")");
    }

    double v = IIOScale * value;
    if (v > Cfg.MaxScaledVoltage) {
        throw std::runtime_error(Cfg.ChannelNumber + " scaled value (" + std::to_string(v) +
                                 ") is bigger than maximum (" +
                                 std::to_string(Cfg.MaxScaledVoltage) + ")");
    }

    double res = v * Cfg.VoltageMultiplier / 1000.0; // got mV let's divide it by 1000 to obtain V

    std::ostringstream out;
    out << std::setprecision(Cfg.DecimalPlaces) << std::fixed << res;
    MeasuredV = out.str();
}

int32_t TChannelReader::ReadFromADC()
{
    std::ifstream adcValStream;
    std::string   fileName = SysfsIIODir + "/in_" + Cfg.ChannelNumber + "_raw";
    OpenWithException(adcValStream, fileName);
    int32_t val;
    adcValStream >> val;
    if (adcValStream.fail())
        throw std::runtime_error("Can't read from " + fileName);
    return val;
}

void TChannelReader::SelectScale(WBMQTT::TLogger& infoLogger)
{
    std::string scalePrefix = SysfsIIODir + "/in_" + Cfg.ChannelNumber + "_scale";

    std::ifstream scaleFile;
    TryOpen({scalePrefix + "_available",
             SysfsIIODir + "/in_voltage_scale_available",
             SysfsIIODir + "/scale_available"},
            scaleFile);

    if (scaleFile.is_open()) {
        auto contents = std::string((std::istreambuf_iterator<char>(scaleFile)),
                                    std::istreambuf_iterator<char>());
        infoLogger.Log() << "Available scales: " << contents;

        std::string bestScaleStr =
            FindBestScale(WBMQTT::StringSplit(contents, " "), Cfg.DesiredScale);

        if (!bestScaleStr.empty()) {
            IIOScale = std::stod(bestScaleStr);
            WriteToFile(scalePrefix, bestScaleStr);
            infoLogger.Log() << scalePrefix << " is set to " << bestScaleStr;
            return;
        }
    }

    // scale_available file is not present read the current scale(in_voltageX_scale) from sysfs or
    // from group scale(in_voltage_scale)
    TryOpen({scalePrefix, SysfsIIODir + "/in_voltage_scale"}, scaleFile);
    if (scaleFile.is_open()) {
        scaleFile >> IIOScale;
    }
    infoLogger.Log() << scalePrefix << " = " << IIOScale;
}

std::string FindSysfsIIODir(const std::string& matchIIO)
{
    if (matchIIO.empty()) {
        return "/sys/bus/iio/devices/iio:device0";
    }

    std::string pattern = "*" + matchIIO + "*";

    auto fn = [&](const std::string& d) {
        char buf[512];
        int  len;
        if ((len = readlink(d.c_str(), buf, 512)) < 0)
            return false;
        buf[len] = 0;
        return (fnmatch(pattern.c_str(), buf, 0) == 0);
    };

    return IterateDir("/sys/bus/iio/devices", "iio:device", fn);
}

std::string FindBestScale(const std::vector<std::string>& scales, double desiredScale)
{
    std::string bestScaleStr;
    double      bestScaleDouble = 0;

    for (auto& scaleStr : scales) {
        double val;
        try {
            val = stod(scaleStr);
        } catch (std::invalid_argument e) {
            continue;
        }
        // best scale is either maximum scale or the one closest to user request
        if (((desiredScale > 0) &&
             (fabs(val - desiredScale) <= fabs(bestScaleDouble - desiredScale))) // user request
            || ((desiredScale <= 0) && (val >= bestScaleDouble))                 // maximum scale
        ) {
            bestScaleDouble = val;
            bestScaleStr    = scaleStr;
        }
    }
    return bestScaleStr;
}