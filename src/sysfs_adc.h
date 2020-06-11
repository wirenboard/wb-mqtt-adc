#pragma once

#include <wblib/log.h>

#include <fstream>

#include "moving_average.h"

#define ADC_DEFAULT_MAX_VOLTAGE 3100 // voltage in mV

/**
 * @brief Iterate over /sys/bus/iio/devices and find folder symlinked to matchIIO value. Throws
 * runtime_error on search failure.
 *
 * @param matchIIO Symlink origin
 * @param sysFsFolder Sys fs folder. By default = /sys
 * @return std::string Found folder or sysFsPrefix + /bus/iio/devices/iio:device0 if matchIIO is
 * empty
 */
std::string FindSysfsIIODir(const std::string& matchIIO, const std::string& sysFsFolder = "/sys");

/**
 * @brief Find best scale from given list. The best scale is either maximum scale or the one closest
 * to user request.
 *
 * @param scales Vector of strings with available scales
 * @param desiredScale Desired scale
 * @return std::string Best found scale or empty string
 */
std::string FindBestScale(const std::vector<std::string>& scales, double desiredScale);

/**
 * @brief The class is responsible for single ADC channel measurements.
 */
class TChannelReader
{
public:
    //! ADC channel measurement settings
    struct TSettings
    {
        //! IIO channel "voltageX"
        std::string ChannelNumber = "voltage1";

        //! Number of value readings during one selection
        uint32_t ReadingsNumber = 10;

        //! Maximum result after multiplying readed value from ADC to Scale
        double MaxScaledVoltage = ADC_DEFAULT_MAX_VOLTAGE;

        /*! The ADC scale to use. The closest supported scale will be used.
            It affects the accuracy and the measurement range.
            If 0, the maximum available scale is used.
        */
        double DesiredScale = 0;

        //! The ADC voltage is multiplied by this factor to get the resulting value
        double VoltageMultiplier = 1;

        //! Number of consecutive readings to average
        uint32_t AveragingWindow = 10;

        //! Number of figures after point
        uint32_t DecimalPlaces = 3;
    };

    /**
     * @brief Construct a new TChannelReader object
     *
     * @param defaultIIOScale Default channel scale if can't get it from sysfs
     * @param maxADCvalue Maximum possible value from ADC
     * @param channelCfg Channel settings from conf file
     * @param delayBetweenMeasurementsmS Delay between mesurements in mS
     * @param debugLogger Logger for debug messages
     * @param infoLogger Logger for info messages
     * @param sysfsIIODir Sysfs device's folder
     */
    TChannelReader(double                           defaultIIOScale,
                   uint32_t                         maxADCvalue,
                   const TChannelReader::TSettings& channelCfg,
                   uint32_t                         delayBetweenMeasurementsmS,
                   WBMQTT::TLogger&                 debugLogger,
                   WBMQTT::TLogger&                 infoLogger,
                   const std::string&               sysfsIIODir);

    //! Get last measured value
    std::string GetValue() const;

    //! Read and convert value from ADC
    void Measure();

private:
    //! Settings for the channel
    TChannelReader::TSettings Cfg;

    //! Last measured voltage in V
    double MeasuredV;

    //! Folder in sysfs corresponding to the channel
    std::string SysfsIIODir;

    /*! Selected scale for the channel.

        The closest value from one of files
        /bus/iio/devices/iio:device0/in_voltageX_scale_available
        /bus/iio/devices/iio:device0/in_voltage_scale_available
        /bus/iio/devices/iio:device0/scale_available

        Or the value from one of
        /bus/iio/devices/iio:device0/in_voltageX_scale
        /bus/iio/devices/iio:device0/in_voltage_scale

        Or the value got from constructor's parameter defaultIIOScale.
    */
    double IIOScale;

    //! Maximum possible value from ADC
    uint32_t MaxADCValue;

    //! Delay between measurements in mS
    uint32_t DelayBetweenMeasurementsmS;

    TMovingAverageCalculator AverageCounter;
    std::ifstream            AdcValStream;
    WBMQTT::TLogger&         DebugLogger;
    WBMQTT::TLogger&         InfoLogger;

    int32_t ReadFromADC();
    void    SelectScale();

    TChannelReader();
};