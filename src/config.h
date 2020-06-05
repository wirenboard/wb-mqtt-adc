#pragma once

#include <string>
#include <vector>

#define ADC_DEFAULT_MAX_VOLTAGE 3100 // voltage in mV

//! ADC channel settings
struct TADCChannel
{
    std::string Id;                                          //! Topic name "/devices/ + TADCDriver::Name + /controls/ + Id"
    std::string ChannelNumber     = "voltage1";              //! IIO channel "voltageX"
    uint32_t    ReadingsCount     = 10;                      //! Count of value readings during one selection
    float       MaxVoltage        = ADC_DEFAULT_MAX_VOLTAGE; //! Maximum measurable voltage
    float       Scale             = 0;                       //! The ADC scale to use. The closest supported scale (from _scale_available list) will be used. It affects the accuracy and the measurement range. If 0, the maximum available scale is used.",
    float       VoltageMultiplier = 1;                       //! The ADC voltage is multiplied to this factor to get the resulting value
    uint32_t    AveragingWindow   = 10;                      //! Count of consecutive readings to average
    uint32_t    DecimalPlaces     = 3;                       //! Count of figures after point
};

//! Programm settings
struct TConfig
{
    std::string              DeviceName = "ADCs";
    bool                     Debug      = false;
    std::vector<TADCChannel> Channels;            //! ADC channels list
};

/**
 * @brief Load configuration
 * 
 * @param mainConfigFile - path and name of a main config file. The function also reads files from mainConfigFile + ".d" folder
 * @param optionalConfigFile - path and name of an optional config file. It will be loaded instead of main config file
 */
TConfig loadConfig(const std::string& mainConfigFile, const std::string& optionalConfigFile);
