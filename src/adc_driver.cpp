#include "adc_driver.h"

#include <vector>

#include "sysfs_adc.h"

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
        bool           Error;
        TChannelReader Reader;

        //! Flag indicating what we should create MQTT control for channel
        bool           ShouldCreateControl = true;
    };

    WBMQTT::TControlArgs MakeControlArgs(const std::string& id, size_t order, const std::string& error)
    {
        return WBMQTT::TControlArgs{}.SetId(id)
                                     .SetType("voltage")
                                     .SetError(error)
                                     .SetOrder(order)
                                     .SetReadonly(true);
    }

    void CreateControl(WBMQTT::PDriverTx&   tx,
                       WBMQTT::TLocalDevice& device,
                       size_t                order,
                       const std::string&    id,
                       const std::string&    value,
                       const std::string&    error)
    {
        device.CreateControl(tx, MakeControlArgs(id, order, error).SetRawValue(value)).Wait();
    }

    void AdcWorker(bool*                                      active,
                   WBMQTT::PLocalDevice                       device,
                   WBMQTT::PDeviceDriver                      mqttDriver,
                   std::shared_ptr<std::vector<TChannelDesc>> channels,
                   size_t                                     controlOrder,
                   WBMQTT::TLogger&                           infoLogger,
                   WBMQTT::TLogger&                           errorLogger)
    {
        bool shouldRemoveUnusedControls = true;
        infoLogger.Log() << "ADC worker thread is started";
        while (*active) {
            for (auto& channel : *channels) {
                channel.Reader.Measure(channel.MqttId + " ");
                try {
                    channel.Error = false;
                } catch (const std::exception& er) {
                    channel.Error = true;
                    errorLogger.Log() << er.what();
                }
            }

            auto tx = mqttDriver->BeginTx();
            for (auto& channel : *channels) {
                if (channel.ShouldCreateControl) {
                    CreateControl(tx, *device, controlOrder, channel.MqttId, channel.Reader.GetValue(), channel.Error ? "r" : "");
                    infoLogger.Log() << "Channel " << channel.MqttId << " MQTT controls are created, poll interval " << channel.Reader.GetInterval() << " ms";
                    ++controlOrder;
                    channel.ShouldCreateControl = false;
                } else {
                    WBMQTT::PControl control = device->GetControl(channel.MqttId);
                    if (channel.Error) {
                        auto future = control->SetError(tx, "r");
                        future.Wait();
                    } else {
                        auto future = control->SetRawValue(tx, channel.Reader.GetValue());
                        future.Wait();
                    }
                }
            }
            if (shouldRemoveUnusedControls) {
                device->RemoveUnusedControls(tx);
                shouldRemoveUnusedControls = false;
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

    size_t controlOrder = 0;
    std::shared_ptr<std::vector<TChannelDesc>> readers(new std::vector<TChannelDesc>());
    for (const auto& channel : config.Channels) {
        std::string sysfsIIODir = FindSysfsIIODir(channel.MatchIIO);
        if (sysfsIIODir.empty()) {
            ErrorLogger.Log() << "Can't fild matching sysfs IIO: " + channel.MatchIIO;
            Device->CreateControl(tx, MakeControlArgs(channel.Id, controlOrder, "r")).Wait();
            ++controlOrder;
        } else {
            readers->push_back(TChannelDesc{channel.Id,
                                            false,
                                            {MXS_LRADC_DEFAULT_SCALE_FACTOR,
                                             MAX_ADC_VALUE,
                                             channel.ReaderCfg,
                                             DebugLogger,
                                             InfoLogger,
                                             sysfsIIODir}});
        }
    }

    Active = true;
    Worker = WBMQTT::MakeThread(
        "ADC worker",
        {[=] { AdcWorker(&Active, Device, MqttDriver, readers, controlOrder, InfoLogger, ErrorLogger); }});
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
