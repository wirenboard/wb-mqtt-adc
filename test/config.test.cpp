#include "src/config.h"
#include <gtest/gtest.h>
#include <vector>
#include "src/log.h"

class TConfigTest : public testing::Test
{
protected:
    std::string testRootDir;
    std::string schemaFile;
    std::string confDDir;

    void SetUp()
    {
        char* d = getenv("TEST_DIR_ABS");
        if (d != NULL) {
            testRootDir = d;
            testRootDir += '/';
        }
        testRootDir += "config_test_data";

        const auto CONFIG_SCHEMA_TEMPLATE_FILE = testRootDir + "/../../data/wb-mqtt-adc-template.schema.json";
        confDDir = testRootDir + "/conf.d";
        schemaFile = testRootDir + "/wb-mqtt-adc.schema.json";
        MakeSchemaForConfed(confDDir, CONFIG_SCHEMA_TEMPLATE_FILE, schemaFile);
    }

    void TearDown()
    {
        remove(schemaFile.c_str());
    }
};

TEST_F(TConfigTest, no_file)
{
    ASSERT_THROW(LoadConfig("fake.conf", "", "", ""), std::runtime_error);
    ASSERT_THROW(LoadConfig("", "a1", "", ""), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/wb-mqtt-adc.conf", "", "", ""), std::runtime_error);
}

TEST_F(TConfigTest, bad_config)
{
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad1.conf", "", "", schemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad2.conf", "", "", schemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad3.conf", "", "", schemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad4.conf", "", "", schemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad5.conf", "", "", schemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig(testRootDir + "/bad/bad6.conf", "", "", schemaFile), std::runtime_error);
    ASSERT_THROW(LoadConfig("", testRootDir + "/bad/bad1.conf", "", schemaFile), std::runtime_error);
}

TEST_F(TConfigTest, optional_config)
{
    TConfig cfg = LoadConfig(testRootDir + "/good1/wb-mqtt-adc.conf",
                             testRootDir + "/good1/optional.conf",
                             confDDir,
                             schemaFile,
                             &InfoLogger,
                             &WarnLogger);
    ASSERT_EQ(cfg.DeviceName, "Test");
    ASSERT_EQ(cfg.EnableDebugMessages, true);
    ASSERT_EQ(cfg.Channels.size(), 1);
    ASSERT_EQ(cfg.Channels[0].Id, "Vin");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.AveragingWindow, 1);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ChannelNumber, "voltage8");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DecimalPlaces, 2);
    ASSERT_EQ(cfg.Channels[0].MatchIIO, "path");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.VoltageMultiplier, 17);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DesiredScale, 5);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.MaxScaledVoltage, 12500);
}

TEST_F(TConfigTest, empty_main_config)
{
    TConfig cfg = LoadConfig(testRootDir + "/good1/wb-mqtt-adc.conf",
                             "",
                             confDDir,
                             schemaFile,
                             &InfoLogger,
                             &WarnLogger);
    ASSERT_EQ(cfg.DeviceName, "ADCs");
    ASSERT_EQ(cfg.EnableDebugMessages, false);
    ASSERT_EQ(cfg.Channels.size(), 1);
    ASSERT_EQ(cfg.Channels[0].Id, "Vin");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.AveragingWindow, 1);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ChannelNumber, "voltage8");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DecimalPlaces, 2);
    ASSERT_EQ(cfg.Channels[0].MatchIIO, "../../../devices/platform/soc/2100000.bus/2198000.adc/iio:device0");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.VoltageMultiplier, 17.66666);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DesiredScale, 0);
}

TEST_F(TConfigTest, full_main_config)
{
    TConfig cfg = LoadConfig(testRootDir + "/good2/wb-mqtt-adc.conf",
                             "",
                             confDDir,
                             schemaFile,
                             &InfoLogger,
                             &WarnLogger);
    ASSERT_EQ(cfg.DeviceName, "ADCs");
    ASSERT_EQ(cfg.EnableDebugMessages, false);
    ASSERT_EQ(cfg.Channels.size(), 1);
    ASSERT_EQ(cfg.Channels[0].Id, "Vin");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.AveragingWindow, 10);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.ChannelNumber, "voltage8");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DecimalPlaces, 20);
    ASSERT_EQ(cfg.Channels[0].MatchIIO, "../../../devices/platform/soc/2100000.bus/2198000.adc/iio:device0");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.VoltageMultiplier, 17.66666);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DesiredScale, 50);
}

TEST_F(TConfigTest, poll_interval_config)
{
    using namespace std::literals::chrono_literals;

    TConfig cfg = LoadConfig(testRootDir + "/good3/wb-mqtt-adc.conf",
                             "",
                             confDDir,
                             schemaFile,
                             &InfoLogger,
                             &WarnLogger);

    ASSERT_EQ(cfg.DeviceName, "ADCs");
    ASSERT_EQ(cfg.EnableDebugMessages, false);
    ASSERT_EQ(cfg.Channels.size(), 2);
    ASSERT_EQ(cfg.Channels[0].Id, "Vin");
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.AveragingWindow, 10);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.PollInterval, 100ms);
    ASSERT_EQ(cfg.Channels[0].ReaderCfg.DelayBetweenMeasurements, 5ms);

    ASSERT_EQ(cfg.Channels[1].Id, "5Vout");
    ASSERT_EQ(cfg.Channels[1].ReaderCfg.AveragingWindow, 10);
    ASSERT_EQ(cfg.Channels[1].ReaderCfg.PollInterval, 100ms);

    // in config file it is set to 50, but must be fixed because
    // averaging will not fit in poll_interval otherwise.
    // 10 is poll_interval / averaging_window
    ASSERT_EQ(cfg.Channels[1].ReaderCfg.DelayBetweenMeasurements, 10ms);
}

TEST_F(TConfigTest, max_unchanged_interval)
{
    ASSERT_THROW(LoadConfig(testRootDir + "/max_unchanged_interval/bad.conf", "", "", schemaFile), std::runtime_error);

    {
        TConfig cfg = LoadConfig(testRootDir + "/max_unchanged_interval/good_empty.conf",
                                "",
                                confDDir,
                                schemaFile,
                                &InfoLogger,
                                &WarnLogger);
        ASSERT_EQ(cfg.MaxUnchangedInterval.count(), 60);
    }

    {
        TConfig cfg = LoadConfig(testRootDir + "/max_unchanged_interval/good_non_empty.conf",
                                "",
                                confDDir,
                                schemaFile,
                                &InfoLogger,
                                &WarnLogger);
        ASSERT_EQ(cfg.MaxUnchangedInterval.count(), 123);
    }

}
