#pragma once

#include <wblib/log.h>

#include <fstream>

#include "moving_average.h"

#define ADC_DEFAULT_MAX_VOLTAGE 3100 // voltage in mV

/**
 * @brief The class is responsible for single ADC channel measurements.
 */
class TChannelReader
{
public:
    //! ADC channel measurement settings
    struct TSettings
    {
        //! Fnmatch-compatible pattern to match with iio:deviceN symlink target
        std::string MatchIIO;
        std::string ChannelNumber    = "voltage1"; //! IIO channel "voltageX"
        uint32_t    ReadingsNumber   = 10;         //! Number of value readings during one selection
        double      MaxScaledVoltage = ADC_DEFAULT_MAX_VOLTAGE; //! Maximum result after multiplying
                                                                //! readed value from ADC to Scale
        double Scale = 0; //! The ADC scale to use. The closest supported scale (from
                          //! _scale_available list) will be used. It affects the accuracy and the
                          //! measurement range. If 0, the maximum available scale is used.",
        double VoltageMultiplier =
            1; //! The ADC voltage is multiplied by this factor to get the resulting value
        uint32_t AveragingWindow = 10; //! Number of consecutive readings to average
        uint32_t DecimalPlaces   = 3;  //! Number of figures after point
    };

    /**
     * @brief Construct a new TChannelReader object
     *
     * @param defaultIIOScale
     * @param maxADCvalue
     * @param channelCfg
     * @param delayBetweenMeasurementsmS
     * @param debugLogger
     * @param sysFsPrefix
     */
    TChannelReader(double                           defaultIIOScale,
                   uint32_t                         maxADCvalue,
                   const TChannelReader::TSettings& channelCfg,
                   uint32_t                         delayBetweenMeasurementsmS,
                   WBMQTT::TLogger&                 debugLogger,
                   WBMQTT::TLogger&                 infoLogger,
                   const std::string&               sysFsPrefix = "/sys");

    std::string GetValue() const;
    void        Measure();

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