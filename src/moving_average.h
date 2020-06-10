#pragma once

#include <vector>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief The class calculates moving average from given values.
 */
class TMovingAverageCalculator
{
        std::vector<int32_t> LastValues;
        int32_t              Sum;
        size_t               Pos;
        bool                 Ready;
    public:
        /**
         * @brief Construct a new TMovingAverageCalculator object
         * 
         * @param windowSize Number of values to average.
         */
        TMovingAverageCalculator(size_t windowSize);

        /**
         * @brief Add new value to data set.
         */
        void AddValue(int32_t value);

        /**
         * @brief Get average value. The value is valid only if IsReady() == true.
         */
        uint32_t Average() const;

        /**
         * @brief Check if average value is valid.
         * 
         * @return true Average value is valid
         * @return false Average value is not valid. The value will be valid after processing of at least windowSize values.
         */
        bool IsReady() const;
};
