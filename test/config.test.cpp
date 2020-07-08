#include "src/config.h"
#include <gtest/gtest.h>
#include <vector>

class TConfigTest : public testing::Test
{
protected:
    std::string testRootDir;
    std::string shemaFile;

    void SetUp()
    {
        char* d = getenv("TEST_DIR_ABS");
        if (d != NULL) {
            testRootDir = d;
            testRootDir += '/';
        }
        testRootDir += "config_test_data";

        shemaFile = testRootDir + "/../../data/wb-mqtt-adc.schema.json";
    }
};

TEST_F(TConfigTest, no_file)
{
    ASSERT_THROW(LoadConfig("fake.conf", "", ""), std::runtime_error);
    ASSERT_THROW(LoadConfig("", "a1", ""), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/wb-mqtt-adc.conf", "", ""), std::runtime_error);
}

TEST_F(TConfigTest, bad_config)
{
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad1.conf", "", shemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad2.conf", "", shemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad3.conf", "", shemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad4.conf", "", shemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad5.conf", "", shemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig("", testRootDir + "/bad/bad1.conf", shemaFile), std::runtime_error);
}

TEST_F(TConfigTest, optional_config)
{
    TConfig cfg = LoadConfig(testRootDir + "/good1/wb-mqtt-adc.conf",
                             testRootDir + "/good1/optional.conf",
                             shemaFile);
    ASSERT_EQ(cfg.DeviceName, "Test");
    ASSERT_EQ(cfg.EnableDebugMessages, true);
    ASSERT_EQ(cfg.Channels.size(), 1);
    ASSERT_EQ(cfg.Channels[0].Id, "Vin");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.AveragingWindow, 1);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ChannelNumber, "voltage8");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DecimalPlaces, 2);
    ASSERT_EQ(cfg.Channels[0].MatchIIO, "path");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ReadingsNumber, 3);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.VoltageMultiplier, 17);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DesiredScale, 5);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.MaxScaledVoltage, 12500);
}

TEST_F(TConfigTest, empty_main_config)
{
    TConfig cfg = LoadConfig(testRootDir + "/good1/wb-mqtt-adc.conf", "", shemaFile);
    ASSERT_EQ(cfg.DeviceName, "ADCs");
    ASSERT_EQ(cfg.EnableDebugMessages, false);
    ASSERT_EQ(cfg.Channels.size(), 1);
    ASSERT_EQ(cfg.Channels[0].Id, "Vin");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.AveragingWindow, 1);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ChannelNumber, "voltage8");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DecimalPlaces, 2);
    ASSERT_EQ(cfg.Channels[0].MatchIIO, "path");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ReadingsNumber, 3);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.VoltageMultiplier, 17);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DesiredScale, 5);
}

TEST_F(TConfigTest, full_main_config)
{
    TConfig cfg = LoadConfig(testRootDir + "/good2/wb-mqtt-adc.conf", "", shemaFile);
    ASSERT_EQ(cfg.DeviceName, "ADCs");
    ASSERT_EQ(cfg.EnableDebugMessages, false);
    ASSERT_EQ(cfg.Channels.size(), 1);
    ASSERT_EQ(cfg.Channels[0].Id, "Vin");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.AveragingWindow, 10);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ChannelNumber, "voltage80");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DecimalPlaces, 20);
    ASSERT_EQ(cfg.Channels[0].MatchIIO, "path0");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ReadingsNumber, 30);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.VoltageMultiplier, 170);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DesiredScale, 50);
}
