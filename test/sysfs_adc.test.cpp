#include "src/sysfs_adc.h"
#include <gtest/gtest.h>
#include <vector>

class TSysfsTest : public testing::Test
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

TEST_F(TSysfsTest, read_value)
{
    WBMQTT::TLogger           logger("", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED, false);
    TChannelReader::TSettings channelCfg{"voltage1", 1, 10000, 2.54, 10.5, 1, 5};
    const double              MAX_SCALE = 2.54;
    TChannelReader            reader(MAX_SCALE, 3100, channelCfg, 10, logger, logger, testRootDir);
    reader.Measure();
    ASSERT_EQ(reader.GetValue(), "6.77418");
}
