#pragma once

#include <functional>
#include <wblib/wbmqtt.h>

#include <thread>

#include "config.h"


class TADCDriver
{
    public:

        TADCDriver(const WBMQTT::PDeviceDriver& mqttDriver, const TConfig& config, WBMQTT::TLogger& errorLogger, WBMQTT::TLogger& debugLogger, WBMQTT::TLogger& infoLogger);

        void Stop();

    private:
        WBMQTT::PDeviceDriver        MqttDriver;
        WBMQTT::PLocalDevice         Device;

        bool                         Active;
        std::mutex                   ActiveMutex;
        std::unique_ptr<std::thread> Worker;

        WBMQTT::TLogger&             ErrorLogger;
        WBMQTT::TLogger&             DebugLogger;
        WBMQTT::TLogger&             InfoLogger;
};