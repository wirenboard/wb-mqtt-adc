#include <gtest/gtest.h>
#include <wblib/testing/testlog.h>
#include "sysfs_w1.h"

using namespace std;
using namespace WBMQTT;
using namespace WBMQTT::Testing;

string test_sensor_root_dir = string(getenv("TEST_DIR_ABS")) + string("/fake_sensors/");

class TMaybeValueTest: public TLoggedFixture
{
    protected:
        void SetUp() {
            TLoggedFixture::SetUp();
        };
        void TearDown() {
            TLoggedFixture::TearDown();
        };
};

TEST_F(TMaybeValueTest, false_object_1)
{
    auto v = TMaybeValue<int>();
    EXPECT_EQ(v.IsDefined(), false);
}

TEST_F(TMaybeValueTest, true_object_1)
{
    auto v = TMaybeValue<int>(12);
    EXPECT_EQ(v.IsDefined(), true);
}

TEST_F(TMaybeValueTest, true_object_2)
{
    auto v = TMaybeValue<int>(12);
    EXPECT_EQ(v.GetValue(), 12);
}

//////// SysfsOnewireDevice test

class TSysfsOnewireDeviceTest: public TLoggedFixture
{
    protected:
        void SetUp() {};
        void TearDown() {};
};

TEST_F(TSysfsOnewireDeviceTest, family)
{
    TSysfsOnewireDevice s = TSysfsOnewireDevice("sensor_name", "asd");
    EXPECT_EQ(s.GetDeviceFamily(), TOnewireFamilyType::ProgResThermometer);
}

TEST_F(TSysfsOnewireDeviceTest, name)
{
    TSysfsOnewireDevice s = TSysfsOnewireDevice("sensor_name", "asd");
    EXPECT_EQ(s.GetDeviceName(),"sensor_name");
}

TEST_F(TSysfsOnewireDeviceTest, operator_true)
{
    TSysfsOnewireDevice s1 = TSysfsOnewireDevice("sensor_name", "asd");
    TSysfsOnewireDevice s2 = TSysfsOnewireDevice("sensor_name", "asd");
    EXPECT_EQ(s1, s2);
}

TEST_F(TSysfsOnewireDeviceTest, operator_false)
{
    TSysfsOnewireDevice s1 = TSysfsOnewireDevice("sensor_name1", "asd");
    TSysfsOnewireDevice s2 = TSysfsOnewireDevice("sensor_name2", "asd");
    EXPECT_EQ( (s1 == s2), false );
}

TEST_F(TSysfsOnewireDeviceTest, 1_sensor_read)
{
    TSysfsOnewireDevice s1 = TSysfsOnewireDevice("28-00000a013d97", test_sensor_root_dir + string("1_sensor/"));
    auto v = s1.ReadTemperature();
    EXPECT_EQ(to_string(v.GetValue()), "26.312000");
}

TEST_F(TSysfsOnewireDeviceTest, no_sensor_read)
{
    TSysfsOnewireDevice s1 = TSysfsOnewireDevice("28-00000a013d97", test_sensor_root_dir + string("no_sensor/"));
    auto v = s1.ReadTemperature();
    EXPECT_EQ(v.IsDefined(), false);
}

TEST_F(TSysfsOnewireDeviceTest, wrong_crc)
{
    TSysfsOnewireDevice s1 = TSysfsOnewireDevice("28-00000a013000-wrong-crc", test_sensor_root_dir + string("2_sensor/"));
    auto v = s1.ReadTemperature();
    EXPECT_EQ(v.IsDefined(), false);
}

//////// SysfsOnewireManager test

class TSysfsOnewireManagerTest: public TLoggedFixture
{
    protected:
        void SetUp() {
            TLoggedFixture::SetUp();
        };
        void TearDown() {
            TLoggedFixture::TearDown();
         };
};

TEST_F(TSysfsOnewireManagerTest, no_sensor)
{ 
    TSysfsOnewireManager manager = TSysfsOnewireManager(test_sensor_root_dir + string("no_sensor"));
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevicesP().size(), 0);
} 

TEST_F(TSysfsOnewireManagerTest, 1_sensor)
{ 
    TSysfsOnewireManager manager = TSysfsOnewireManager(test_sensor_root_dir + string("1_sensor"));
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevicesP().size(), 1);
} 

TEST_F(TSysfsOnewireManagerTest, 2_sensor)
{ 
    TSysfsOnewireManager manager = TSysfsOnewireManager(test_sensor_root_dir + string("2_sensor"));
    manager.RescanBus();
    EXPECT_EQ(manager.GetDevicesP().size(), 2);
} 