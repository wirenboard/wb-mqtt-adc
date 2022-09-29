#include "sysfs_adc.h"

#include <chrono>
#include <fnmatch.h>
#include <iomanip>
#include <math.h>
#include <unistd.h>

#include <wblib/utils.h>

#include "file_utils.h"

TChannelReader::TChannelReader(double                           defaultIIOScale,
                               const TChannelReader::TSettings& cfg,
                               WBMQTT::TLogger&                 debugLogger,
                               WBMQTT::TLogger&                 infoLogger,
                               const std::string&               sysfsIIODir)
    : Cfg(cfg), SysfsIIODir(sysfsIIODir), IIOScale(defaultIIOScale),
      AverageCounter(cfg.AveragingWindow), DebugLogger(debugLogger), InfoLogger(infoLogger),
      LastMeasureTimestamp(Timestamp::min()), NextPollTimestamp(Timestamp::min()),
      FirstPollInLoopTimestamp(Timestamp::min())
{
    SelectScale(infoLogger);
}

std::string TChannelReader::GetValue() const
{
    return MeasuredV;
}

TChannelReader::Timestamp TChannelReader::GetLastMeasureTimestamp() const
{
    return LastMeasureTimestamp;
}

TChannelReader::Timestamp TChannelReader::GetNextPollTimestamp() const
{
    return NextPollTimestamp;
}

std::chrono::milliseconds TChannelReader::GetDelayBetweenMeasurements() const
{
    return Cfg.DelayBetweenMeasurements;
}

std::chrono::milliseconds TChannelReader::GetPollInterval() const
{
    return Cfg.PollInterval;
}

void TChannelReader::Poll(Timestamp now, const std::string& debugMessagePrefix)
{
    // skip this poll if scheduled time is not come yet
    if (NextPollTimestamp > now) {
        return;
    }

    // if it is out very first poll, save its start time
    if (FirstPollInLoopTimestamp == Timestamp::min()) {
        FirstPollInLoopTimestamp = now;
    }

    if (NextPollTimestamp == Timestamp::min()) {
        NextPollTimestamp = now;
    }

    // perform scheduled measurement
    int32_t adcMeasurement = ReadFromADC();
    DebugLogger.Log() << debugMessagePrefix << Cfg.ChannelNumber << " = " << adcMeasurement;
    AverageCounter.AddValue(adcMeasurement);

    // if average counter is awaiting more data, schedule next poll and exit
    if (!AverageCounter.IsReady()) {
        NextPollTimestamp = NextPollTimestamp + Cfg.DelayBetweenMeasurements;
        return;
    }

    // average counter collected enough data, publish it
    int32_t value = AverageCounter.GetAverage();
    double v = IIOScale * value;
    if (v > Cfg.MaxScaledVoltage) {
        throw std::runtime_error(debugMessagePrefix + Cfg.ChannelNumber + " scaled value (" + std::to_string(v) + ") is bigger than maximum (" +
                                 std::to_string(Cfg.MaxScaledVoltage) + ")");
    }

    double res = v * Cfg.VoltageMultiplier / 1000.0; // got mV let's divide it by 1000 to obtain V

    std::ostringstream out;
    out << std::setprecision(Cfg.DecimalPlaces) << std::fixed << res;
    MeasuredV = out.str();

    // update current measurement timestamp info and schedule next one
    NextPollTimestamp = FirstPollInLoopTimestamp + Cfg.PollInterval;
    FirstPollInLoopTimestamp = NextPollTimestamp;

    LastMeasureTimestamp = now;

    // reset accumulator
    AverageCounter.Reset();
}

int32_t TChannelReader::ReadFromADC()
{
    std::string fileName = SysfsIIODir + "/in_" + Cfg.ChannelNumber + "_raw";
    for (size_t i = 0; i < 3; ++i) { // workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53984
        {
            std::ifstream adcValStream;
            OpenWithException(adcValStream, fileName);
            int32_t val;
            try {
                adcValStream >> val;
                if (adcValStream.good())
                    return val;
            } catch (const std::ios_base::failure& er) {
                DebugLogger.Log() << er.what() << " " << fileName;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Can't read from " + fileName);
}

void TChannelReader::SelectScale(WBMQTT::TLogger& infoLogger)
{
    std::string scalePrefix = SysfsIIODir + "/in_" + Cfg.ChannelNumber + "_scale";

    std::ifstream scaleFile;
    TryOpen({scalePrefix + "_available", SysfsIIODir + "/in_voltage_scale_available", SysfsIIODir + "/scale_available"}, scaleFile);

    if (scaleFile.is_open()) {
        auto contents = std::string((std::istreambuf_iterator<char>(scaleFile)), std::istreambuf_iterator<char>());
        infoLogger.Log() << "Available scales: " << contents;

        std::string bestScaleStr = FindBestScale(WBMQTT::StringSplit(contents, " "), Cfg.DesiredScale);

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

    return IterateDirByPattern("/sys/bus/iio/devices", "iio:device", fn);
}

std::string FindBestScale(const std::vector<std::string>& scales, double desiredScale)
{
    std::string bestScaleStr;
    double      bestScaleDouble = 0;

    for (auto& scaleStr : scales) {
        double val;
        try {
            val = stod(scaleStr);
        } catch (const std::invalid_argument& e) {
            continue;
        }
        // best scale is either maximum scale or the one closest to user request
        if (((desiredScale > 0) && (fabs(val - desiredScale) <= fabs(bestScaleDouble - desiredScale))) // user request
            || ((desiredScale <= 0) && (val >= bestScaleDouble))                                       // maximum scale
        ) {
            bestScaleDouble = val;
            bestScaleStr    = scaleStr;
        }
    }
    return bestScaleStr;
}
