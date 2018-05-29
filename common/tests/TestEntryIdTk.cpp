#include <common/toolkit/EntryIdTk.h>

#include <gtest/gtest.h>

TEST(EntryIdTk, isValidHexToken)
{
    using EntryIdTk::isValidHexToken;

   EXPECT_TRUE(isValidHexToken("0"));
   EXPECT_FALSE(isValidHexToken(""));
   EXPECT_TRUE(isValidHexToken(std::string(8, '0')));
   EXPECT_FALSE(isValidHexToken(std::string(9, '0')));
   EXPECT_TRUE(isValidHexToken("123ADB"));
   EXPECT_FALSE(isValidHexToken("123ADBS"));
}

TEST(EntryIdTk, isValidEntryIdFormat)
{
   using EntryIdTk::isValidEntryIdFormat;

   EXPECT_TRUE(isValidEntryIdFormat("0-59F03330-1"));

   EXPECT_TRUE(isValidEntryIdFormat("0-0-0"));
   EXPECT_FALSE(isValidEntryIdFormat("0-0-0-0"));
   EXPECT_FALSE(isValidEntryIdFormat("0-0"));
   EXPECT_FALSE(isValidEntryIdFormat("-0"));
   EXPECT_FALSE(isValidEntryIdFormat("0-"));
   EXPECT_FALSE(isValidEntryIdFormat("--"));
}
