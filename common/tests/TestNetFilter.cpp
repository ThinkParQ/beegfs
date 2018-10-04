#include <common/toolkit/NetFilter.h>

#include <gtest/gtest.h>

class TestNetFilter : public ::testing::Test {
   protected:
      std::string fileName;

      void createFile(const char* content)
      {
         fileName = "/tmp/XXXXXX";
         int fd = mkstemp(&fileName[0]);
         ASSERT_GE(fd, 0);
         ASSERT_EQ(write(fd, content, strlen(content)), ssize_t(strlen(content)));
         close(fd);
      }

      void TearDown() override
      {
         unlink(fileName.c_str());
      }
};

TEST_F(TestNetFilter, noFile)
{
   NetFilter filter("");

   ASSERT_EQ(filter.getNumFilterEntries(), 0u);

   // always allow loopback
   EXPECT_TRUE(filter.isAllowed(htonl(INADDR_LOOPBACK)));
   EXPECT_TRUE(filter.isContained(htonl(INADDR_LOOPBACK)));

   // can't check for all ips, try a random one
   EXPECT_TRUE(filter.isAllowed(htonl(0xd5c1a899)));
   EXPECT_FALSE(filter.isContained(htonl(0xd5c1a899)));
}

TEST_F(TestNetFilter, netmask0)
{
   createFile("10.0.0.0/0");

   NetFilter filter(fileName);

   ASSERT_EQ(filter.getNumFilterEntries(), 1u);

   // always allow loopback
   EXPECT_TRUE(filter.isAllowed(htonl(INADDR_LOOPBACK)));
   EXPECT_TRUE(filter.isContained(htonl(INADDR_LOOPBACK)));

   // 10.0.0.x should definitely be allowed
   EXPECT_TRUE(filter.isAllowed(htonl(0x0a010203)));
   EXPECT_TRUE(filter.isContained(htonl(0x0a010203)));

   // netfilter /0 says everything else is allowed too
   EXPECT_TRUE(filter.isAllowed(htonl(0xd5c1a899)));
   EXPECT_TRUE(filter.isContained(htonl(0xd5c1a899)));
}

TEST_F(TestNetFilter, netmaskExclusive)
{
   createFile("10.0.0.0/24\n10.1.0.0/24");

   NetFilter filter(fileName);

   ASSERT_EQ(filter.getNumFilterEntries(), 2u);

   // always allow loopback
   EXPECT_TRUE(filter.isAllowed(htonl(INADDR_LOOPBACK)));
   EXPECT_TRUE(filter.isContained(htonl(INADDR_LOOPBACK)));

   // 10.0.0.x should definitely be allowed
   EXPECT_TRUE(filter.isAllowed(htonl(0x0a000003)));
   EXPECT_TRUE(filter.isContained(htonl(0x0a000003)));
   // 10.0.1.x should not be allowed
   EXPECT_FALSE(filter.isAllowed(htonl(0x0a000103)));
   EXPECT_FALSE(filter.isContained(htonl(0x0a000103)));

   // same thing for 10.1.0.x
   EXPECT_TRUE(filter.isAllowed(htonl(0x0a010003)));
   EXPECT_TRUE(filter.isContained(htonl(0x0a010003)));
   EXPECT_FALSE(filter.isAllowed(htonl(0x0a010103)));
   EXPECT_FALSE(filter.isContained(htonl(0x0a010103)));

   // other addresses are forbidden
   EXPECT_FALSE(filter.isAllowed(htonl(0xd5c1a899)));
   EXPECT_FALSE(filter.isContained(htonl(0xd5c1a899)));
}
