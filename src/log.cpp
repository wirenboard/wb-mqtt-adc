#include "log.h"

WBMQTT::TLogger ErrorLogger("ERROR: [wb-adc] ", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED);
WBMQTT::TLogger DebugLogger("DEBUG: [wb-adc] ", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::WHITE, false);
WBMQTT::TLogger InfoLogger("INFO: [wb-adc] ", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::GREY);
WBMQTT::TLogger WarnLogger("WARNING: [wb-adc] ", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::YELLOW);