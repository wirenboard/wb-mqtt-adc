/*
#include <mosquittopp.h>
#include <fstream>
#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include "sysfs_adc.h"

using namespace std;

struct THandlerConfig
{
    std::string DeviceName = "Adcs";
    bool Debug = false;
    vector<TChannel> Channels;
};

class TMQTTAdcHandler : public TMQTTWrapper
{
public:
    explicit TMQTTAdcHandler(const TMQTTAdcHandler::TConfig& mqtt_config, const THandlerConfig handler_config) ;

    void OnConnect(int rc) ;
    void OnMessage(const struct mosquitto_message *message);
    void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    std::string GetChannelTopic(const TSysfsAdcChannel& channel) const;
    void UpdateChannelValues();
    virtual void UpdateValue() ;
private:
    THandlerConfig Config;
    vector<std::unique_ptr<TSysfsAdc>> AdcHandlers;
    vector<std::shared_ptr<TSysfsAdcChannel>> Channels;
};
*/

#pragma once

#include <wblib/wbmqtt.h>

#include <thread>

#include "config.h"


class TADCDriver
{
    public:

        static const char * const Name;

        TADCDriver(const WBMQTT::PDeviceDriver& mqttDriver, const TConfig& config);
        ~TADCDriver();

        void Start();
        void Stop();
        void Clear() noexcept;

    private:
        WBMQTT::PDeviceDriver  MqttDriver;
        WBMQTT::PLocalDevice   Device;

        std::atomic_bool                    Active;
        std::atomic_bool                    Cleaned;
        std::unique_ptr<std::thread>        Worker;
};