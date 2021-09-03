#include "src/sysfs_adc.h"
#include <gtest/gtest.h>
#include <vector>
#include <chrono>

class TSysfsTest : public testing::Test
{
protected:
    std::string testRootDir;
    std::string schemaFile;

    void SetUp()
    {
        char* d = getenv("TEST_DIR_ABS");
        if (d != NULL) {
            testRootDir = d;
            testRootDir += '/';
        }
        testRootDir += "sysfs_test_data";
    }
};

TEST_F(TSysfsTest, scale_finder)
{
    ASSERT_EQ(FindBestScale(std::vector<std::string>(), 100), std::string());
    ASSERT_EQ(FindBestScale({"a", "b", "c"}, 100), std::string());
    ASSERT_EQ(FindBestScale({"1.23"}, 100), "1.23");
    ASSERT_EQ(FindBestScale({"1.23", "101.22222"}, 100), "101.22222");
    ASSERT_EQ(FindBestScale({"1.23", "a", "101.22222", "b", "99"}, 100), "99");
}

TEST_F(TSysfsTest, read_value_single_measurement)
{
    WBMQTT::TLogger           logger("", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED, false);
    TChannelReader::TSettings channelCfg{"voltage1", 10000, 2.54, 10.5, 1, 5};
    const double              MAX_SCALE = 2.54;
    TChannelReader            reader(MAX_SCALE, 3100, channelCfg, logger, logger, testRootDir);

    auto now = std::chrono::steady_clock::now();
    reader.Poll(now);

    ASSERT_EQ(reader.GetLastMeasureTimestamp(), now);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.PollInterval);

    ASSERT_EQ(reader.GetValue(), "6.77418");
}

TEST_F(TSysfsTest, read_value_average_measurement)
{
    WBMQTT::TLogger           logger("", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED, false);
    TChannelReader::TSettings channelCfg{"voltage1", 10000, 2.54, 10.5, 3, 5};
    const double              MAX_SCALE = 2.54;
    TChannelReader            reader(MAX_SCALE, 3100, channelCfg, logger, logger, testRootDir);

    auto now = std::chrono::steady_clock::now();

    reader.Poll(now);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.DelayBetweenMeasurements);

    reader.Poll(now + channelCfg.DelayBetweenMeasurements);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.DelayBetweenMeasurements * 2);

    reader.Poll(now + channelCfg.DelayBetweenMeasurements * 2);
    ASSERT_EQ(reader.GetLastMeasureTimestamp(), now + channelCfg.DelayBetweenMeasurements * 2);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.PollInterval);

    ASSERT_EQ(reader.GetValue(), "6.77418");
}
