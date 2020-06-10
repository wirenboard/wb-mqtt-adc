#include "moving_average.h"

#include <math.h>

TMovingAverageCalculator::TMovingAverageCalculator(size_t windowSize): Sum(0), Pos(0), Ready(false) {
    LastValues.resize(windowSize);
}

void TMovingAverageCalculator::AddValue(int32_t value)
{
    Sum -= (Ready ? LastValues[Pos] : 0);
    LastValues[Pos] = value;
    Sum += value;
    ++Pos;
    Pos %= LastValues.size();
    if(Pos == 0) {
        Ready = true;
    }
}

uint32_t TMovingAverageCalculator::Average() const
{
    return std::round(Sum / (double)LastValues.size());
}

bool TMovingAverageCalculator::IsReady() const
{
    return Ready;
}
