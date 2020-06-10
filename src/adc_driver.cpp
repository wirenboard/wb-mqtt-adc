#include "adc_driver.h"

#include "sysfs_adc.h"
#include <vector>

/*
"/devices/" DriverId "/meta/name"                                       = Config.DeviceName
"/devices/" DriverId "/controls/" Config.Channels[i].Id                 = measured voltage
"/devices/" DriverId "/controls/" Config.Channels[i].Id "/meta/order"   = i from Config.Channels[i]
"/devices/" DriverId "/controls/" Config.Channels[i].Id "/meta/type"    = string "voltage"
*/

//! default scale for file "in_voltageNUMBER_scale"
#define MXS_LRADC_DEFAULT_SCALE_FACTOR 0.451660156

namespace
{
    const char* DriverId = "wb-adc";

    struct TChannelDesc
    {
        std::string    MqttId;
        TChannelReader Reader;
    };

    void AdcWorker(bool*                                      active,
                   WBMQTT::PLocalDevice                       device,
                   WBMQTT::PDeviceDriver                      mqttDriver,
                   std::shared_ptr<std::vector<TChannelDesc>> channels,
                   WBMQTT::TLogger&                           infoLogger)
    {
        infoLogger.Log() << "ADC worker thread is started";
        while (*active) {
            for (auto& channel : *channels) {
                channel.Reader.Measure();
            }

            auto tx = mqttDriver->BeginTx();
            for (const auto& channel : *channels) {
                device->GetControl(channel.MqttId)->SetRawValue(tx, channel.Reader.GetValue());
            }
        }
        infoLogger.Log() << "ADC worker thread is stopped";
    }
} // namespace

TADCDriver::TADCDriver(const WBMQTT::PDeviceDriver& mqttDriver,
                       const TConfig&               config,
                       WBMQTT::TLogger&             errorLogger,
                       WBMQTT::TLogger&             debugLogger,
                       WBMQTT::TLogger&             infoLogger)
    : MqttDriver(mqttDriver), ErrorLogger(errorLogger), DebugLogger(debugLogger),
      InfoLogger(infoLogger)
{
    InfoLogger.Log() << "Creating driver MQTT controls";
    auto tx = MqttDriver->BeginTx();
    Device  = tx->CreateDevice(WBMQTT::TLocalDeviceArgs{}
                                  .SetId(DriverId)
                                  .SetTitle(config.DeviceName)
                                  .SetIsVirtual(true)
                                  .SetDoLoadPrevious(false))
                 .GetValue();

    size_t n = 0;

    std::shared_ptr<std::vector<TChannelDesc>> readers(new std::vector<TChannelDesc>());
    for (const auto& channel : config.Channels) {
        infoLogger.Log() << "Adding MQTT controls";
        auto futureControl = Device->CreateControl(tx,
                                                   WBMQTT::TControlArgs{}
                                                       .SetId(channel.Id)
                                                       .SetType("voltage")
                                                       .SetOrder(n)
                                                       .SetReadonly(true));
        ++n;

        // FIXME: delay ???
        readers->push_back(TChannelDesc{channel.Id,
                                        {MXS_LRADC_DEFAULT_SCALE_FACTOR,
                                         ADC_DEFAULT_MAX_VOLTAGE,
                                         channel.ReaderCfg,
                                         10,
                                         DebugLogger,
                                         InfoLogger}});
        futureControl.Wait();
        infoLogger.Log() << "Channel " << channel.Id << " MQTT controls are created";
    }

    Active = true;
    Worker =
        WBMQTT::MakeThread("ADC worker",
                           {[=] { AdcWorker(&Active, Device, MqttDriver, readers, InfoLogger); }});
}

void TADCDriver::Stop()
{
    {
        std::lock_guard<std::mutex> lg(ActiveMutex);
        if (!Active) {
            ErrorLogger.Log() << "Attempt to stop already stopped TADCDriver";
            return;
        }
        Active = false;
    }

    InfoLogger.Log() << "Stopping...";

    if (Worker->joinable()) {
        Worker->join();
    }
    Worker.reset();

    try {
        MqttDriver->BeginTx()->RemoveDeviceById(DriverId).Sync();
    } catch (const std::exception& e) {
        ErrorLogger.Log() << "Exception during TADCDriver::Stop: " << e.what();
    } catch (...) {
        ErrorLogger.Log() << "Unknown exception during TADCDriver::Stop";
    }
}
