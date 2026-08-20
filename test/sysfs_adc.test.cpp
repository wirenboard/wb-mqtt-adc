#include "sysfs_adc.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <limits.h>
#include <stdlib.h>
#include <vector>

class TSysfsTest: public testing::Test
{
protected:
    std::string SysfsTestDir;

    void SetUp()
    {
        InitTestSysfsDataDir();
    }

    void InitTestSysfsDataDir()
    {
        char tempdir_template[PATH_MAX];
        strncpy(tempdir_template, "/tmp/wb-mqtt-adc-test.XXXXXX", PATH_MAX);
        char* tempdir = ::mkdtemp(tempdir_template);
        if (!tempdir) {
            FAIL() << "failed to create temp dir: " << strerror(errno);
            return;
        }
        SysfsTestDir = std::string(tempdir) + "/";

        WriteTestSysfsFile("in_voltage1_raw", "254");
        WriteTestSysfsFile("in_voltage_scale", "2.54");
    }

    void WriteTestSysfsFile(const std::string& filename, const std::string& data)
    {
        std::string path = SysfsTestDir + "/" + filename;
        std::ofstream f(path);
        if (!f.is_open()) {
            FAIL() << "failed opening " << path << ": " << strerror(errno);
            return;
        }

        f << data << std::endl;
        f.close();
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
    WBMQTT::TLogger logger("", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED, false);
    TChannelReader::TSettings channelCfg{"voltage1", 10000, 2.54, 10.5, 1, 5};
    const double MAX_SCALE = 2.54;
    TChannelReader reader(MAX_SCALE, channelCfg, logger, logger, SysfsTestDir);

    auto now = std::chrono::steady_clock::now();
    reader.Poll(now);

    ASSERT_EQ(reader.GetLastMeasureTimestamp(), now);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.PollInterval);

    ASSERT_EQ(reader.GetValue(), "6.77418");
}

TEST_F(TSysfsTest, read_value_average_measurement)
{
    WBMQTT::TLogger logger("", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED, false);
    TChannelReader::TSettings channelCfg{"voltage1", 10000, 2.54, 10.5, 3, 5};
    const double MAX_SCALE = 2.54;
    TChannelReader reader(MAX_SCALE, channelCfg, logger, logger, SysfsTestDir);

    auto now = std::chrono::steady_clock::now();

    WriteTestSysfsFile("in_voltage1_raw", "254");
    reader.Poll(now);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.DelayBetweenMeasurements);

    WriteTestSysfsFile("in_voltage1_raw", "250");
    reader.Poll(now + channelCfg.DelayBetweenMeasurements);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.DelayBetweenMeasurements * 2);

    WriteTestSysfsFile("in_voltage1_raw", "258");
    reader.Poll(now + channelCfg.DelayBetweenMeasurements * 2);
    ASSERT_EQ(reader.GetLastMeasureTimestamp(), now + channelCfg.DelayBetweenMeasurements * 2);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.PollInterval);

    ASSERT_EQ(reader.GetValue(), "6.77418");
}

TEST_F(TSysfsTest, overvoltage_error_reschedules_next_poll)
{
    WBMQTT::TLogger logger("", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED, false);
    // 254 * 2.54 = 645.16 mV is above the 100 mV threshold
    TChannelReader::TSettings channelCfg{"voltage1", 100, 2.54, 10.5, 1, 5};
    const double MAX_SCALE = 2.54;
    TChannelReader reader(MAX_SCALE, channelCfg, logger, logger, SysfsTestDir);

    auto now = std::chrono::steady_clock::now();
    ASSERT_THROW(reader.Poll(now), std::runtime_error);

    // the failed cycle must be rescheduled, otherwise the caller polls in a tight loop
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.PollInterval);
    ASSERT_EQ(reader.GetLastMeasureTimestamp(), TChannelReader::Timestamp::min());

    // and nothing is read until the next cycle is due
    reader.Poll(now + channelCfg.PollInterval / 2);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.PollInterval);
}

TEST_F(TSysfsTest, read_error_reschedules_next_poll_and_drops_accumulated_data)
{
    WBMQTT::TLogger logger("", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED, false);
    TChannelReader::TSettings channelCfg{"voltage1", 10000, 2.54, 10.5, 3, 5};
    const double MAX_SCALE = 2.54;
    TChannelReader reader(MAX_SCALE, channelCfg, logger, logger, SysfsTestDir);

    auto now = std::chrono::steady_clock::now();
    reader.Poll(now);
    ASSERT_EQ(reader.GetNextPollTimestamp(), now + channelCfg.DelayBetweenMeasurements);

    auto failedAt = now + channelCfg.DelayBetweenMeasurements;
    ASSERT_EQ(::remove((SysfsTestDir + "/in_voltage1_raw").c_str()), 0);
    ASSERT_THROW(reader.Poll(failedAt), std::runtime_error);
    ASSERT_EQ(reader.GetNextPollTimestamp(), failedAt + channelCfg.PollInterval);
    ASSERT_EQ(reader.GetLastMeasureTimestamp(), TChannelReader::Timestamp::min());

    // the sample taken before the error is dropped, so a full window is needed again
    WriteTestSysfsFile("in_voltage1_raw", "254");
    auto cycleStart = failedAt + channelCfg.PollInterval;
    reader.Poll(cycleStart);
    reader.Poll(cycleStart + channelCfg.DelayBetweenMeasurements);
    ASSERT_EQ(reader.GetLastMeasureTimestamp(), TChannelReader::Timestamp::min());

    reader.Poll(cycleStart + channelCfg.DelayBetweenMeasurements * 2);
    ASSERT_EQ(reader.GetLastMeasureTimestamp(), cycleStart + channelCfg.DelayBetweenMeasurements * 2);
    ASSERT_EQ(reader.GetValue(), "6.77418");
}
