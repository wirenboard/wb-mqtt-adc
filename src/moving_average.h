#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

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
     * @param windowSize Number of values to average. If windowSize == 0 std::runtime_error is thrown
     */
    TMovingAverageCalculator(size_t windowSize);

    /**
     * @brief Add new value to data set.
     */
    void AddValue(int32_t value);

    /**
     * @brief Get average value. The value is valid only if IsReady() == true.
     */
    int32_t GetAverage() const;

    /**
     * @brief Check if average value is valid. The value is valid after processing of at least
     * windowSize values.
     *
     * @return true Average value is valid
     * @return false Average value is not valid
     */
    bool IsReady() const;

    /**
     * @brief Clear accumulator and prepare for new loop
     */
    void Reset();
};
