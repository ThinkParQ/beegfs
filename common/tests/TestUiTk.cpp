#include <common/toolkit/UiTk.h>

#include <sstream>

#include <gtest/gtest.h>

bool testQuestion(const std::string& answer, boost::optional<bool> defaultAnswer=boost::none)
{
   std::stringstream input(answer);
   return uitk::userYNQuestion("Test", defaultAnswer, input);
}

TEST(UiTk, userYNQuestion)
{
   // basic input
   ASSERT_TRUE( testQuestion("Y"));
   ASSERT_FALSE(testQuestion("N"));

   ASSERT_TRUE( testQuestion("y"));
   ASSERT_FALSE(testQuestion("n"));

   ASSERT_TRUE( testQuestion("Yes"));
   ASSERT_FALSE(testQuestion("No"));

   // default answers
   ASSERT_TRUE( testQuestion("", true));
   ASSERT_FALSE(testQuestion("", false));

   ASSERT_TRUE( testQuestion("Y", false));
   ASSERT_TRUE( testQuestion("Y", false));
   ASSERT_FALSE(testQuestion("N", true));
   ASSERT_FALSE(testQuestion("N", true));
}
