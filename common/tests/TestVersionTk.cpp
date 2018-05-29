#include <common/app/log/LogContext.h>
#include <common/toolkit/VersionTk.h>

#include <gtest/gtest.h>

TEST(VersionTk, encodeAndDecode)
{
   // encode/decode major/minor/releaseNum with low numbers
   {
      unsigned encodeMajor = 6;
      unsigned encodeMinor = 8;
      unsigned encodeReleaseNum = 1;

      unsigned encodedVersion = BEEGFS_VERSION_NUM_ENCODE(encodeMajor, encodeMinor,
         encodeReleaseNum);

      unsigned decodeMajor;
      unsigned decodeMinor;
      unsigned decodeReleaseNum;

      BEEGFS_VERSION_NUM_DECODE(encodedVersion, decodeMajor, decodeMinor, decodeReleaseNum);

      ASSERT_EQ(encodeMajor, decodeMajor);
      ASSERT_EQ(encodeMinor, decodeMinor);
      ASSERT_EQ(encodeReleaseNum, decodeReleaseNum);
   }

   // encode/decode major/minor/releaseNum with high numbers
   {
      unsigned encodeMajor = 222;
      unsigned encodeMinor = 12;
      unsigned encodeReleaseNum = 211;

      unsigned encodedVersion = BEEGFS_VERSION_NUM_ENCODE(encodeMajor, encodeMinor,
         encodeReleaseNum);

      unsigned decodeMajor;
      unsigned decodeMinor;
      unsigned decodeReleaseNum;

      BEEGFS_VERSION_NUM_DECODE(encodedVersion, decodeMajor, decodeMinor, decodeReleaseNum);

      ASSERT_EQ(encodeMajor, decodeMajor);
      ASSERT_EQ(encodeMinor, decodeMinor);
      ASSERT_EQ(encodeReleaseNum, decodeReleaseNum);
   }
}

TEST(VersionTk, comparison)
{
   // the version string from the fhgfs-version script for this code is: "17.03-beta1"
   unsigned generatedVersionCode = 34669313;
   unsigned generatedVersionNum = BEEGFS_VERSION_NUM_STRIP(generatedVersionCode);

   // values: A < B < C < generatedVersionNum < D < E

   unsigned encodedVersionA = BEEGFS_VERSION_NUM_ENCODE(8, 2, 10);
   unsigned encodedVersionB = BEEGFS_VERSION_NUM_ENCODE(8, 10, 2);
   unsigned encodedVersionC = BEEGFS_VERSION_NUM_ENCODE(16, 10, 2);
   unsigned encodedVersionD = BEEGFS_VERSION_NUM_ENCODE(17, 6, 1);
   unsigned encodedVersionE = BEEGFS_VERSION_NUM_ENCODE(225, 12, 222);

   // simple low-level direct comparison

   ASSERT_LT(encodedVersionA, encodedVersionB);
   ASSERT_LT(encodedVersionB, encodedVersionC);
   ASSERT_LT(encodedVersionC, generatedVersionNum);
   ASSERT_LT(generatedVersionNum, encodedVersionD);
   ASSERT_LT(encodedVersionD, encodedVersionE);

   // compare with VersionTk helper method

   ASSERT_TRUE(VersionTk::checkRequiredRelease(encodedVersionB, encodedVersionB));
   ASSERT_TRUE(VersionTk::checkRequiredRelease(generatedVersionCode, generatedVersionCode));

   ASSERT_TRUE(VersionTk::checkRequiredRelease(encodedVersionB, encodedVersionC));
   ASSERT_TRUE(VersionTk::checkRequiredRelease(encodedVersionC, encodedVersionD));
   ASSERT_TRUE(VersionTk::checkRequiredRelease(encodedVersionA, generatedVersionCode));

   ASSERT_FALSE(VersionTk::checkRequiredRelease(encodedVersionC, encodedVersionB));
   ASSERT_FALSE(VersionTk::checkRequiredRelease(generatedVersionCode, encodedVersionA));
}

