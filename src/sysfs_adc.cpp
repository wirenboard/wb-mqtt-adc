#include "sysfs_adc.h"

#include <fstream>
#include <math.h>
#include <sstream>

using namespace std;

/*
    "/devices/" TADCDriver::Name "/meta/name"                                       = Config.DeviceName
    "/devices/" TADCDriver::Name "/controls/" Config.Channels[i].Id                 = measured voltage
    "/devices/" TADCDriver::Name "/controls/" Config.Channels[i].Id "/meta/order"   = i from Config.Channels[i]
    "/devices/" TADCDriver::Name "/controls/" Config.Channels[i].Id "/meta/type"    = string "voltage"
*/

#define MXS_LRADC_DEFAULT_SCALE_FACTOR 0.451660156 // default scale for file "in_voltageNUMBER_scale"


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
    string bestScaleStr;
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

void OpenWithException(std::ofstream& f, const std::string& fileName)
{
    f.open(fileName);
    if (!f.is_open()) {
        throw std::runtime_error("Can't open file:" + fileName);
    }
}

void OpenWithException(std::ifstream& f, const std::string& fileName)
{
    f.open(fileName);
    if (!f.is_open()) {
        throw std::runtime_error("Can't open file:" + fileName);
    }
}

void WriteToFile(const std::string& fileName, const std::string& value)
{
    ofstream f;
    OpenWithException(f, fileName);
    f << value;
}

TAverageCounter::TAverageCounter(size_t windowSize): Sum(0), Pos(0), Ready(false) {
    LastValues.resize(windowSize);
}

void TAverageCounter::AddValue(uint32_t value)
{
    Sum -= (Ready ? LastValues[Pos] : 0);
    LastValues[Pos] = value;
    Sum += value;
    ++Pos;
    Pos %= LastValues.size();
    if(Pos == 0) {
        Ready = true;
    }
}

uint32_t TAverageCounter::Average() const
{
    return round(Sum / (double)LastValues.size());
}

bool TAverageCounter::IsReady() const
{
    return Ready;
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

        string bestScaleStr = FindBestScale(WBMQTT::StringSplit(contents, " "), Cfg.Scale);

        if(!bestScaleStr.empty()) {
            IIOScale = stod(bestScaleStr);
            WriteToFile(scalePrefix, bestScaleStr);
        }
        return;
    }

    // scale_available file is not present read the current scale(in_voltageX_scale) from sysfs or from group scale(in_voltage_scale)
    TryOpen({ scalePrefix, SysfsIIODir + "/in_voltage_scale" }, scaleFile);
    if (scaleFile.is_open()) {
        scaleFile >> IIOScale;
    }
}

TChannelReader::TChannelReader(double defaultIIOScale, uint32_t maxADCvalue, const TChannelReader::TSettings& cfg, uint32_t delayBetweenMeasurementsmS, const std::string& SysFsPrefix): 
    Cfg(cfg),
    MeasuredV(0.0f),
    IIOScale(defaultIIOScale),
    MaxADCValue(maxADCvalue),
    DelayBetweenMeasurementsmS(delayBetweenMeasurementsmS),
    AverageCounter(cfg.AveragingWindow)
{
    SysfsIIODir = SysFsPrefix + "/bus/iio/devices/iio:device0";

    OpenWithException(AdcValStream, SysfsIIODir + "/in_" + Cfg.ChannelNumber + "_raw");

    SelectScale();
}

double TChannelReader::GetValue() const
{
    return MeasuredV;
}

void TChannelReader::Measure()
{
    MeasuredV = std::nan("");

    for (uint32_t i = 0; i < Cfg.ReadingsCount; ++i) {
        AverageCounter.AddValue(ReadFromADC());
        this_thread::sleep_for(chrono::milliseconds(DelayBetweenMeasurementsmS));
    }

    if(!AverageCounter.IsReady())
        return;

    uint32_t value = AverageCounter.Average();
    if (value > MaxADCValue) 
        return;
    
    // FIXME: check why is it divided by 1000
    double v = IIOScale * value * Cfg.VoltageMultiplier / 1000;
    if (v <= Cfg.MaxVoltageV) {
        MeasuredV = v;
    }
}
