#include "moving_average.h"
#include <gtest/gtest.h>

TEST(TMovingAverageTest, zero_window)
{
    ASSERT_THROW(TMovingAverageCalculator c(0), std::runtime_error);
}

TEST(TMovingAverageTest, one_window)
{
    TMovingAverageCalculator c(1);
    EXPECT_EQ(c.IsReady(), false);
    for (uint32_t i = 0; i < 10; ++i) {
        c.AddValue(i);
        EXPECT_EQ(c.IsReady(), true);
        EXPECT_EQ(c.GetAverage(), i);
    }
}

TEST(TMovingAverageTest, ten_window)
{
    const uint32_t windowSize = 10;
    TMovingAverageCalculator c(windowSize);
    EXPECT_EQ(c.IsReady(), false);
    uint32_t sum = 0;
    for (uint32_t i = 0; i < windowSize; ++i) {
        sum += i * 10;
        c.AddValue(i * 10);
        EXPECT_EQ(c.IsReady(), (i == windowSize - 1));
    }
    EXPECT_EQ(c.GetAverage(), sum / windowSize);

    for (uint32_t i = 0; i < 5; ++i) {
        c.AddValue(100);
    }
    EXPECT_EQ(c.GetAverage(), (100 + 100 + 100 + 100 + 100 + 90 + 80 + 70 + 60 + 50) / 10);
}

TEST(TMovingAverageTest, rounding)
{
    TMovingAverageCalculator c(3);
    c.AddValue(10);
    c.AddValue(10);
    c.AddValue(2);
    EXPECT_EQ(c.GetAverage(), 7);
    c.AddValue(10);
    c.AddValue(10);
    c.AddValue(3);
    EXPECT_EQ(c.GetAverage(), 8);
}

TEST(TMovingAverageTest, negative)
{
    TMovingAverageCalculator c(3);
    c.AddValue(-10);
    c.AddValue(-10);
    c.AddValue(-2);
    EXPECT_EQ(c.GetAverage(), -7);
    c.AddValue(-10);
    c.AddValue(-10);
    c.AddValue(-3);
    EXPECT_EQ(c.GetAverage(), -8);
}
