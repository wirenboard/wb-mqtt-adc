#include "src/file_utils.h"
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>

class TFileUtilsTest : public testing::Test
{
protected:
    std::string testRootDir;

    void SetUp()
    {
        char* d = getenv("TEST_DIR_ABS");
        if (d != NULL) {
            testRootDir = d;
            testRootDir += '/';
        }
        testRootDir += "file_utils_test_data";
    };
};

TEST_F(TFileUtilsTest, no_dir)
{
    auto fn = [](const std::string& f) { return true; };
    ASSERT_THROW(IterateDirByPattern(testRootDir + "/nothing", "test", fn), TNoDirError);
}

TEST_F(TFileUtilsTest, no_match)
{
    auto fn = [](const std::string& f) { return true; };
    ASSERT_EQ(IterateDirByPattern(testRootDir + "/test1", "adc", fn), std::string());
}

TEST_F(TFileUtilsTest, one_match)
{
    std::string res(testRootDir + "/test1/dummy2");
    auto        fn = [&](const std::string& f) { return f == res; };
    ASSERT_EQ(IterateDirByPattern(testRootDir + "/test1", "dummy2", fn), res);

    res = testRootDir + "/test1/test3";
    ASSERT_EQ(IterateDirByPattern(testRootDir + "/test1", "test3", fn), res);
}

TEST_F(TFileUtilsTest, two_matches)
{
    std::vector<std::string> res;

    auto fn = [&](const std::string& f) {
        res.push_back(f);
        return false;
    };
    IterateDirByPattern(testRootDir + "/test1", "dummy", fn);
    ASSERT_EQ(res.size(), 2);

    std::sort(res.begin(), res.end());
    std::vector<std::string> resMatch = {testRootDir + "/test1/dummy",
                                         testRootDir + "/test1/dummy2"};
    ASSERT_EQ(res[0], resMatch[0]);
    ASSERT_EQ(res[1], resMatch[1]);
}
