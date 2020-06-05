#include "adc_driver.h"

#include <iostream>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <iomanip>
#include <ifstream>
#include "sysfs_prefix.h"

using namespace std;

/*
    "/devices/" TADCDriver::Name "/meta/name"                                       = Config.DeviceName
    "/devices/" TADCDriver::Name "/controls/" Config.Channels[i].Id                 = measured voltage
    "/devices/" TADCDriver::Name "/controls/" Config.Channels[i].Id "/meta/order"   = i from Config.Channels[i]
    "/devices/" TADCDriver::Name "/controls/" Config.Channels[i].Id "/meta/type"    = string "voltage"
*/

#define MXS_LRADC_DEFAULT_SCALE_FACTOR 0.451660156 // default scale for file "in_voltageNUMBER_scale"

namespace
{

    bool TryOpen(std::vector<std::string> fnames, std::ifstream& file)
    {
        for (auto& fname : fnames) {
            file.open(fname);
            if (file.is_open()) {
                return;
            }
            file.clear();
        }
    }

    class TChannelReader
    {
            TADCChannel ChannelCfg;
            double Value;
            std::string SysfsIIODir;
            double IIOScale;

            int GetAverageValue()
            {
                if (!d->Ready) {
                    for (int i = 0; i < d->ChannelAveragingWindow; ++i) {
                        d->Owner->SelectMuxChannel(d->Index);
                        int v = d->Owner->ReadValue();

                        d->Buffer[i] = v;
                        d->Sum += v;
                    }
                    d->Ready = true;
                } else {
                    for (int i = 0; i < d->ReadingsNumber; i++) {
                        d->Owner->SelectMuxChannel(d->Index);
                        int v = d->Owner->ReadValue();
                        d->Sum -= d->Buffer[d->Pos];
                        d->Sum += v;
                        d->Buffer[d->Pos++] = v;
                        d->Pos %= d->ChannelAveragingWindow;
                        this_thread::sleep_for(chrono::milliseconds(DELAY));
                    }
                }
                return round(d->Sum / d->ChannelAveragingWindow);
            }

            int ReadValue()
            {
                int val;
                AdcValStream.seekg(0);
                AdcValStream >> val;
                return val;
            }

            bool CheckVoltage(int value)
            {
                double voltage = IIOScale * value;
                if (voltage > MaxVoltage) {
                    return false;
                }
                return true;
            }

            void SetScale(const std::string& scalePrefix, const std::string& value)
            {
                ofstream f(scalePrefix);
                if (!f.is_open()) {
                    throw TAdcException("error opening IIO sysfs scale file");
                }
                f << value;
            }

            std::string FindBestScale(const std::list<std::string>& scales, double desiredScale)
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

            void SelectScale()
            {
                std::string scalePrefix = SysfsIIODir + "/in_" + ChannelCfg.ChannelNumber + "_scale";

                std::ifstream scaleFile;
                TryOpen({ scalePrefix + "_available", 
                        SysfsIIODir + "/in_voltage_scale_available",
                        SysfsIIODir + "/scale_available"
                        }, scaleFile);

                if (scaleFile.is_open()) {
                    auto contents = std::string((std::istreambuf_iterator<char>(scaleFile)), std::istreambuf_iterator<char>());

                    string bestScaleStr = FindBestScale(StringSplit(contents, " "), ChannelConfig.Scale);

                    if(!bestScaleStr.empty()) {
                        IIOScale = stod(bestScaleStr);
                        SetScale(scalePrefix, bestScaleStr);
                    }
                    return;
                }

                // scale_available file is not present read the current scale(in_voltageX_scale) from sysfs or from group scale(in_voltage_scale)
                TryOpen({ scalePrefix, SysfsIIODir + "/in_voltage_scale" }, scaleFile);
                if (scaleFile.is_open()) {
                    scaleFile >> IIOScale;
                }
            }

        public:
            TChannelReader(const TADCChannel& channelCfg): ChannelCfg(channelCfg), Value(0.0f)
            {

                SysfsIIODir = SysfsDir + "/bus/iio/devices/iio:device0";

                std::string path_to_value = SysfsIIODir + "/in_" + ChannelCfg.ChannelNumber + "_raw";
                AdcValStream.open(path_to_value);
                if (!AdcValStream.is_open()) {
                    throw TAdcException("error opening sysfs Adc file");
                }
                
                SelectScale();
            }

            const std::string& GetMqttId() const
            {
                return ChannelCfg.Id;
            }

            double GetValue() const
            {
                return Value;
            }

            void Measure()
            {
                Value = std::nan("");
                int value = GetAverageValue();
                if (value < ADC_VALUE_MAX) {
                    if (CheckVoltage(value)) {
                        Value = (float) value * Multiplier / 1000; // set voltage to V from mV
                        Value *= d->Owner->IIOScale;
                    }
                }
            }
    };

    void AdcWorker(bool& active, WBMQTT::PLocalDevice device, WBMQTT::PDeviceDriver mqttDriver)
    {
        LOG(Info) << "Started";
        std::vector<TChannelReader> channels;
        while (active) {
            for (auto& channel: channels) {
                channel.Measure();
            }

            auto tx     = mqttDriver->BeginTx();
            for (const auto& channel: channels) {
                //LOG(Info) << "Publish: " << names[i];
                device->GetControl(channel.GetMqttId())->SetValue(tx, channel.GetValue());
            }
        }
        LOG(Info) << "Stopped";
    }
}

const char * const TADCDriver::Name = "wb-adc";

TADCDriver::TADCDriver(const WBMQTT::PDeviceDriver& mqttDriver, const TConfig& config): MqttDriver(mqttDriver), Config(config) 
{
    auto tx = MqttDriver->BeginTx();
    Device = tx->CreateDevice(WBMQTT::TLocalDeviceArgs{}
        .SetId(Name)
        .SetTitle(config.DeviceName)
        .SetIsVirtual(true)
        .SetDoLoadPrevious(false)
    ).GetValue();

    size_t n = 0;
    for (const auto & channel: config.Channels) {

        auto futureControl = Device->CreateControl(tx, WBMQTT::TControlArgs{}
                        .SetId(channel.Id)
                        .SetType("voltage")
                        .SetOrder(n)
                        .SetReadonly(true)
                    );
        ++n;
        futureControl.Wait();
    }
}

void TADCDriver::Start() 
{
    if (Active.load()) {
        wb_throw(TW1SensorDriverException, "attempt to start already started driver");
    }

    Active.store(true);
    Cleaned.store(false);
    Worker = WBMQTT::MakeThread("ADC worker", {[this]{
        LOG(Info) << "Started";

        while (Active.load()) {

                    for (unsigned int i = 0; i < names.size(); i++) {
            LOG(Info) << "Publish: " << names[i];
            device->GetControl(names[i])->SetValue(tx, static_cast<double>(values[i]));
        }
                auto start_time = chrono::steady_clock::now();

                // update available sensor list, and mqtt control channels
                UpdateDevicesAndControls();      

                // read available sensors and publish sensor values
                UpdateSensorValues();
            }
            LOG(Info) << "Stopped";
        }
    });

}
