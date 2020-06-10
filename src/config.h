#pragma once

#include "sysfs_adc.h"
#include <string>
#include <vector>

//! ADC channel settings
struct TADCChannelSettings
{
    std::string               Id;        //! Topic name "/devices/DRIVER_NAME/controls/ + Id"
    TChannelReader::TSettings ReaderCfg; //! Parameters of reading and converting measured value
};

//! Programm settings
struct TConfig
{
    std::string DeviceName          = "ADCs";  //! Value of /devices/DRIVER_NAME/meta/name
    bool        EnableDebugMessages = false;   //! Enable logging of debug messages
    std::vector<TADCChannelSettings> Channels; //! ADC channels list
};

/**
 * @brief Load configuration from config files
 *
 * @param mainConfigFile - path and name of a main config file. The function also reads files from
 * mainConfigFile + ".d" folder
 * @param optionalConfigFile - path and name of an optional config file. It will be loaded instead
 * of all other config files
 * @param shemaFile - path and name of a file with JSONShema for configs
 */
TConfig LoadConfig(const std::string& mainConfigFile,
                   const std::string& optionalConfigFile,
                   const std::string& shemaFile);
