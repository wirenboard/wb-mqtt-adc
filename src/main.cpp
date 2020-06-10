#include <iostream>
#include <getopt.h>
#include <chrono>

#include <functional>
#include <wblib/wbmqtt.h>
#include <wblib/log.h>
#include <wblib/signal_handling.h>

#include "config.h"
#include "adc_driver.h"

using namespace std;

WBMQTT::TLogger ErrorLogger("ERROR: [wb-adc] ",   WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED);
WBMQTT::TLogger DebugLogger("DEBUG: [wb-adc] ",   WBMQTT::TLogger::StdErr, WBMQTT::TLogger::WHITE, false);
WBMQTT::TLogger InfoLogger("INFO: [wb-adc] ",   WBMQTT::TLogger::StdErr, WBMQTT::TLogger::GREY);

namespace 
{
    //! Maximum timeout before forced application termination. Topic cleanup can take a lot of time
    const auto DRIVER_STOP_TIMEOUT_S = chrono::seconds(5);

    //! Maximun time to start application. Exceded timeout will case application termination.
    const auto DRIVER_INIT_TIMEOUT_S = chrono::seconds(5);
}

int main(int argc, char *argv[])
{
    WBMQTT::TMosquittoMqttConfig mqttConfig {};
    mqttConfig.Id = "wb-adc";
    WBMQTT::TPromise<void> initialized;

    WBMQTT::SetThreadName("main");
    WBMQTT::SignalHandling::Handle({ SIGINT, SIGTERM });
    WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
        WBMQTT::SignalHandling::Stop();
    });

    /* if signal arrived before driver is initialized:
        wait some time to initialize and then exit gracefully
        else if timed out: exit with error
    */
    WBMQTT::SignalHandling::SetWaitFor(DRIVER_INIT_TIMEOUT_S, initialized.GetFuture(), [&]{
        ErrorLogger.Log() << "Driver takes too long to initialize. Exiting.";
        cerr << "Error: DRIVER_INIT_TIMEOUT_S" << endl;
        exit(1); 
    });

    /* if handling of signal takes too much time: exit with error */
    WBMQTT::SignalHandling::SetOnTimeout(DRIVER_STOP_TIMEOUT_S, [&]{
        ErrorLogger.Log() << "Driver takes too long to stop. Exiting.";
        cerr << "Error: DRIVER_STOP_TIMEOUT_S" << endl;
        exit(2);
    });
    WBMQTT::SignalHandling::Start();

    bool forceDebug = false;
    string customConfig;
    int c;
    while ( (c = getopt(argc, argv, "dc:h:p:")) != -1) {
        switch (c) {
            case 'c': {
                customConfig = optarg;
                cout << "custom config file " << customConfig << endl;
                break;
            }
            case 'p': {
                mqttConfig.Port = stoi(optarg);
                break;
            }
            case 'h': {
                mqttConfig.Host = optarg;
                break;
            }
            case 'd': {
                cout << "debug mode is enabled" << endl;
                forceDebug = true;
                break;
            }
            case '?':
            default:
                break;
        }
    }
    cout << "MQTT broker " << mqttConfig.Host << ':' << mqttConfig.Port << endl;

    try {
        auto mqttDriver = WBMQTT::NewDriver(WBMQTT::TDriverArgs{}
            .SetBackend(WBMQTT::NewDriverBackend(WBMQTT::NewMosquittoMqttClient(mqttConfig)))
            .SetId(mqttConfig.Id)
            .SetUseStorage(false)
            .SetReownUnknownDevices(true)
        );

        mqttDriver->StartLoop();
        WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
            mqttDriver->StopLoop();
            mqttDriver->Close();
        });

        mqttDriver->WaitForReady();

        TConfig config = LoadConfig("/etc/wb-homa-adc.conf", customConfig, "/usr/share/wb-mqtt-confed/schemas/wb-homa-adc.schema.json");

        DebugLogger.SetEnabled(forceDebug || config.Debug);

        TADCDriver driver(mqttDriver, config, ErrorLogger, DebugLogger, InfoLogger);

        WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
            driver.Stop();
        });

        initialized.Complete();
        WBMQTT::SignalHandling::Wait();

    } catch (const std::exception & e) {
        ErrorLogger.Log() << "FATAL: " << e.what();
        WBMQTT::SignalHandling::Stop();
        return 1;
    }

	return 0;
}
