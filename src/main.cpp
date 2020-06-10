#include <chrono>
#include <getopt.h>
#include <iostream>

#include <functional>
#include <wblib/log.h>
#include <wblib/signal_handling.h>
#include <wblib/wbmqtt.h>

#include "adc_driver.h"
#include "config.h"

using namespace std;
using namespace WBMQTT;

TLogger ErrorLogger("ERROR: [wb-adc] ", TLogger::StdErr, TLogger::RED);
TLogger DebugLogger("DEBUG: [wb-adc] ", TLogger::StdErr, TLogger::WHITE, false);
TLogger InfoLogger("INFO: [wb-adc] ", TLogger::StdErr, TLogger::GREY);

namespace
{
    //! Maximum timeout before forced application termination. Topic cleanup can take a lot of time
    const auto DRIVER_STOP_TIMEOUT_S = chrono::seconds(5);

    //! Maximun time to start application. Exceded timeout will case application termination.
    const auto DRIVER_INIT_TIMEOUT_S = chrono::seconds(5);
} // namespace

int main(int argc, char* argv[])
{
    TMosquittoMqttConfig mqttConfig{};
    mqttConfig.Id = "wb-adc";
    TPromise<void> initialized;

    SetThreadName("main");
    SignalHandling::Handle({SIGINT, SIGTERM});
    SignalHandling::OnSignals({SIGINT, SIGTERM}, [&] { SignalHandling::Stop(); });

    /* if signal arrived before driver is initialized:
        wait some time to initialize and then exit gracefully
        else if timed out: exit with error
    */
    SignalHandling::SetWaitFor(DRIVER_INIT_TIMEOUT_S, initialized.GetFuture(), [&] {
        ErrorLogger.Log() << "Driver takes too long to initialize. Exiting.";
        cerr << "Error: DRIVER_INIT_TIMEOUT_S" << endl;
        exit(1);
    });

    /* if handling of signal takes too much time: exit with error */
    SignalHandling::SetOnTimeout(DRIVER_STOP_TIMEOUT_S, [&] {
        ErrorLogger.Log() << "Driver takes too long to stop. Exiting.";
        cerr << "Error: DRIVER_STOP_TIMEOUT_S" << endl;
        exit(2);
    });
    SignalHandling::Start();

    bool   forceDebug = false;
    string customConfig;
    int    c;
    while ((c = getopt(argc, argv, "dc:h:p:")) != -1) {
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
        auto mqttDriver =
            NewDriver(TDriverArgs{}
                          .SetBackend(NewDriverBackend(NewMosquittoMqttClient(mqttConfig)))
                          .SetId(mqttConfig.Id)
                          .SetUseStorage(false)
                          .SetReownUnknownDevices(true));

        mqttDriver->StartLoop();
        SignalHandling::OnSignals({SIGINT, SIGTERM}, [&] {
            mqttDriver->StopLoop();
            mqttDriver->Close();
        });

        mqttDriver->WaitForReady();

        TConfig config = LoadConfig("/etc/wb-homa-adc.conf",
                                    customConfig,
                                    "/usr/share/wb-mqtt-confed/schemas/wb-homa-adc.schema.json");

        DebugLogger.SetEnabled(forceDebug || config.EnableDebugMessages);

        TADCDriver driver(mqttDriver, config, ErrorLogger, DebugLogger, InfoLogger);

        SignalHandling::OnSignals({SIGINT, SIGTERM}, [&] { driver.Stop(); });

        initialized.Complete();
        SignalHandling::Wait();

    } catch (const exception& e) {
        ErrorLogger.Log() << "FATAL: " << e.what();
        SignalHandling::Stop();
        return 1;
    }

    return 0;
}
