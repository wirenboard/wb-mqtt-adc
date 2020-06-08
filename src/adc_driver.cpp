#include "adc_driver.h"

#include <vector>
#include "sysfs_adc.h"

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
    struct TChannelDesc
    {
        std::string    MqttId;
        TChannelReader Reader;
    };

    void AdcWorker(bool* active, WBMQTT::PLocalDevice device, WBMQTT::PDeviceDriver mqttDriver, std::shared_ptr<std::vector<TChannelDesc> > channels)
    {
        while (*active) {
            for (auto& channel: *channels) {
                channel.Reader.Measure();
            }

            auto tx = mqttDriver->BeginTx();
            for (const auto& channel: *channels) {
                device->GetControl(channel.MqttId)->SetRawValue(tx, channel.Reader.GetValue());
            }
        }
    }
}

TADCDriver::TADCDriver(const WBMQTT::PDeviceDriver& mqttDriver, const TConfig& config, WBMQTT::TLogger& debugLogger, WBMQTT::TLogger& infoLogger): 
    MqttDriver(mqttDriver),
    DebugLogger(debugLogger),
    InfoLogger(infoLogger)
{
    auto tx = MqttDriver->BeginTx();
    Device = tx->CreateDevice(WBMQTT::TLocalDeviceArgs{}
        .SetId("wb-adc")
        .SetTitle(config.DeviceName)
        .SetIsVirtual(true)
        .SetDoLoadPrevious(false)
    ).GetValue();

    size_t n = 0;
    
    std::shared_ptr<std::vector<TChannelDesc> > readers(new std::vector<TChannelDesc>());
    for (const auto & channel: config.Channels) {
        auto futureControl = Device->CreateControl(tx, WBMQTT::TControlArgs{}
                        .SetId(channel.Id)
                        .SetType("voltage")
                        .SetOrder(n)
                        .SetReadonly(true)
                    );
        ++n;

        // FIXME: delay ???
        readers->push_back(TChannelDesc{channel.Id, {MXS_LRADC_DEFAULT_SCALE_FACTOR, ADC_DEFAULT_MAX_VOLTAGE, channel.ReaderCfg, 10, DebugLogger}});
        futureControl.Wait();
    }

    Cleaned.store(false);

    Active = true;
    Worker = WBMQTT::MakeThread("ADC worker", {[=]{AdcWorker(&Active, Device, MqttDriver, readers);}});
}

TADCDriver::~TADCDriver()
{

}

void TADCDriver::Stop()
{

}
