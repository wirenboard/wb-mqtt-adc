#pragma once

#include <functional>
#include <wblib/wbmqtt.h>

#include <thread>
#include <fstream>

#define ADC_DEFAULT_MAX_VOLTAGE 3100 // voltage in mV

class TAverageCounter
{
        std::vector<uint32_t> LastValues;
        uint32_t Sum;
        size_t Pos;
        bool Ready;
    public:
        TAverageCounter(size_t windowSize);

        void AddValue(uint32_t value);

        uint32_t Average() const;

        bool IsReady() const;
};

/**
 * @brief Class is responsible for single ADC channel measurements.
 */
class TChannelReader
{
    public:
        //! ADC channel measurement settings
        struct TSettings
        {
            std::string ChannelNumber     = "voltage1";                        //! IIO channel "voltageX"
            uint32_t    ReadingsCount     = 10;                                //! Count of value readings during one selection
            double      MaxVoltageV       = ADC_DEFAULT_MAX_VOLTAGE / 1000.0f; //! Maximum measurable voltage in V
            double      Scale             = 0;                                 //! The ADC scale to use. The closest supported scale (from _scale_available list) will be used. It affects the accuracy and the measurement range. If 0, the maximum available scale is used.",
            double      VoltageMultiplier = 1;                                 //! The ADC voltage is multiplied by this factor to get the resulting value
            uint32_t    AveragingWindow   = 10;                                //! Count of consecutive readings to average
            uint32_t    DecimalPlaces     = 3;                                 //! Count of figures after point
        };

        TChannelReader(double defaultIIOScale, uint32_t maxADCvalue, const TChannelReader::TSettings& channelCfg, uint32_t delayBetweenMeasurementsmS, const std::string& sysFsPrefix = "/sys");

        double GetValue() const;
        void Measure();

    private:
            //! Settings for the channel
        TChannelReader::TSettings Cfg;

        //! Last measured voltage in V
        double MeasuredV;

        //! Folder in sysfs corresponding to the channel
        std::string  SysfsIIODir;
        
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

        TAverageCounter AverageCounter;

        std::ifstream AdcValStream;

        uint32_t ReadFromADC();
        void SelectScale();

        TChannelReader();
};