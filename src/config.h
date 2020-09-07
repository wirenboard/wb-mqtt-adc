#pragma once

#include "sysfs_adc.h"
#include <string>
#include <vector>

//! ADC channel settings
struct TADCChannelSettings
{
    //! Topic name "/devices/DRIVER_NAME/controls/ + Id"
    std::string Id;

    //! Fnmatch-compatible pattern to match with iio:deviceN symlink target
    std::string MatchIIO;

    //! Parameters of reading and converting measured value
    TChannelReader::TSettings ReaderCfg;
};

//! Programm settings
struct TConfig
{
    std::string DeviceName          = "ADCs";  //! Value of /devices/DRIVER_NAME/meta/name
    bool        EnableDebugMessages = false;   //! Enable logging of debug messages
    std::vector<TADCChannelSettings> Channels; //! ADC channels list
};

//! Validation error class
class TBadConfigError : public std::runtime_error
{
public:
    TBadConfigError(const std::string& msg);
};

/**
 * @brief Load configuration from config files. Throws TBadConfigError on validation error.
 *
 * @param mainConfigFile - path and name of a main config file. The function also reads files from
 * mainConfigFile + ".d" folder
 * @param optionalConfigFile - path and name of an optional config file. It will be loaded instead
 * of all other config files
 * @param schemaFile - path and name of a file with JSONSchema for configs
 */
TConfig LoadConfig(const std::string& mainConfigFile,
                   const std::string& optionalConfigFile,
                   const std::string& schemaFile);
