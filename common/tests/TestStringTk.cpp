#include <common/toolkit/StringTk.h>

#include <list>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(StringTk, implode)
{
   EXPECT_EQ(std::string(""),      StringTk::implode(",", std::vector<int>()));
   EXPECT_EQ(std::string("1"),     StringTk::implode(",", std::vector<int>({1})));
   EXPECT_EQ(std::string("1,2,3"), StringTk::implode(",", std::vector<int>({1,2,3})));
}
