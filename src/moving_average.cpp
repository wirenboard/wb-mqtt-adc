#include "moving_average.h"

#include <cmath>
#include <stdexcept>

TMovingAverageCalculator::TMovingAverageCalculator(size_t windowSize)
{
    if (windowSize == 0) {
        throw std::runtime_error("Moving average window size can't be zero");
    }
    LastValues.resize(windowSize);
    Reset();
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

int32_t TMovingAverageCalculator::GetAverage() const
{
    return std::round(Sum / (double)LastValues.size());
}

bool TMovingAverageCalculator::IsReady() const
{
    return Ready;
}

void TMovingAverageCalculator::Reset()
{
    Ready = false;
    Pos = 0;
    Sum = 0;
}
