#pragma once

#include "sysfs_adc.h"
#include <iostream>
#include <string>
#include <vector>
#include <wblib/json_utils.h>
#include <wblib/log.h>

//! ADC channel settings
struct TADCChannelSettings
{
    //! Topic name "/devices/DRIVER_NAME/controls/ + Id"
    std::string Id;

    //! Value of /devices/DRIVER_NAME/controls/ + Id + /meta title (empty - not set)
    std::string Title;

    //! Fnmatch-compatible pattern to match with iio:deviceN symlink target
    std::string MatchIIO;

    //! Parameters of reading and converting measured value
    TChannelReader::TSettings ReaderCfg;
};

//! Programm settings
struct TConfig
{
    std::string DeviceName = "ADCs";  //! Value of /devices/DRIVER_NAME/meta/name
    bool EnableDebugMessages = false; //! Enable logging of debug messages
    std::chrono::seconds MaxUnchangedInterval = std::chrono::seconds(60);
    std::vector<TADCChannelSettings> Channels; //! ADC channels list
};

/**
 * @brief Load configuration from config files. Throws std::runtime_error on validation error.
 *
 * @param mainConfigFile - path and name of a main config file.
 * It will be loaded if optional config file is empty.
 * @param optionalConfigFile - path and name of an optional config file. It will be loaded instead
 * of all other config files
 * @param systemConfigsDir - folder with system generated config files.
 * They will be loaded if optional config file is empty.
 * @param schemaFile - path and name of a file with JSONSchema for configs
 */
TConfig LoadConfig(const std::string& mainConfigFile,
                   const std::string& optionalConfigFile,
                   const std::string& systemConfigsDir,
                   const std::string& schemaFile,
                   WBMQTT::TLogger* infoLogger = nullptr,
                   WBMQTT::TLogger* warnLogger = nullptr);

//! Make JSON for confed from the main config and system configs
Json::Value MakeJsonForConfed(const std::string& configFile,
                              const std::string& systemConfigsDir,
                              const std::string& schemaFile);

//! Make main config from JSON received from confed
Json::Value MakeConfigFromConfed(const Json::Value& confedConfig,
                                 const std::string& systemConfigsDir,
                                 const std::string& schemaFile);

//! Write JSON for confed made by MakeJsonForConfed to the stream
void WriteJsonForConfed(const std::string& configFile,
                        const std::string& systemConfigsDir,
                        const std::string& schemaFile,
                        std::ostream& out);

//! Read JSON from confed from the input stream and write main config made by MakeConfigFromConfed to the output stream
void WriteConfigFromConfed(std::istream& in,
                           const std::string& systemConfigsDir,
                           const std::string& schemaFile,
                           std::ostream& out);

void MakeSchemaForConfed(const std::string& systemConfigsDir,
                         const std::string& schemaFile,
                         const std::string& schemaForConfedFile);
