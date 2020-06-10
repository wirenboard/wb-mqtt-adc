#include "moving_average.h"

#include <math.h>
#include <stdexcept>

TMovingAverageCalculator::TMovingAverageCalculator(size_t windowSize) : Sum(0), Pos(0), Ready(false)
{
    if(windowSize == 0) {
        throw std::runtime_error("Moving average window size can't be zero");
    }
    LastValues.resize(windowSize);
}

void TMovingAverageCalculator::AddValue(int32_t value)
{
    Sum -= (Ready ? LastValues[Pos] : 0);
    LastValues[Pos] = value;
    Sum += value;
    ++Pos;
    Pos %= LastValues.size();
    if (Pos == 0) {
        Ready = true;
    }
}

uint32_t TMovingAverageCalculator::GetAverage() const
{
    return std::round(Sum / (double)LastValues.size());
}

bool TMovingAverageCalculator::IsReady() const
{
    return Ready;
}
