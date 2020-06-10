#pragma once

#include <string>
#include <vector>
#include "sysfs_adc.h"

//! ADC channel settings
struct TADCChannelSettings
{
    std::string Id;                      //! Topic name "/devices/ + TADCDriver::Name + /controls/ + Id"
    TChannelReader::TSettings ReaderCfg; //! Parameters of reading and converting measured value
};

//! Programm settings
struct TConfig
{
    std::string                      DeviceName = "ADCs";
    bool                             Debug      = false;
    std::vector<TADCChannelSettings> Channels;            //! ADC channels list
};

/**
 * @brief Load configuration
 * 
 * @param mainConfigFile - path and name of a main config file. The function also reads files from mainConfigFile + ".d" folder
 * @param optionalConfigFile - path and name of an optional config file. It will be loaded instead of main config file
 * @param shemaFile - path and name of a file with JSONShema for configs
 */
TConfig LoadConfig(const std::string& mainConfigFile, const std::string& optionalConfigFile, const std::string& shemaFile);
