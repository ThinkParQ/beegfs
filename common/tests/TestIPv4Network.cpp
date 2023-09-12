#include <common/net/sock/NetworkInterfaceCard.cpp>

#include <gtest/gtest.h>


struct TestIPv4NetworkParseResult
{
   const char* cidr;
   bool valid;
   struct in_addr address;
   uint8_t prefix;
   struct in_addr netmask;
};

struct TestIPv4NetworkParseResult const __TESTIPV4NETWORK_PARSE_RESULTS[] =
{
   {"0.0.0.0/0", true, in_addr_ctor(0), 0, in_addr_ctor(0)},
   {"10.0.0.0/8", true, in_addr_ctor(0x0a), 8, in_addr_ctor(0x0ff)},
   {"10.11.0.0/16", true, in_addr_ctor(0x0b0a), 16, in_addr_ctor(0x0ffff)},
   {"10.11.12.0/24", true, in_addr_ctor(0x0c0b0a), 24, in_addr_ctor(0x0ffffff)},
   {"10.11.12.1/32", true, in_addr_ctor(0x010c0b0a), 32, in_addr_ctor(0x0ffffffff)},
   {"10.11.12.1/", false},
   {"10.11.12.1/33", false},
   {"10.11.12.1/-1", false},
   {"foo.com/16", false},
   {"", false},
   {"/", false},
   {"10.11.12.1/16", true, in_addr_ctor(0x0b0a), 16, in_addr_ctor(0x0ffff)},
   {NULL}
};



class TestIPv4Network : public ::testing::Test
{
};

TEST_F(TestIPv4Network, parseCidr)
{
   for (int i = 0; __TESTIPV4NETWORK_PARSE_RESULTS[i].cidr != NULL; ++i)
   {
      auto& r = __TESTIPV4NETWORK_PARSE_RESULTS[i];
      IPv4Network n;
      //std::cerr << "parse " << r.cidr << std::endl;
      bool v = IPv4Network::parse(r.cidr, n);
      EXPECT_EQ(r.valid, v) << r.cidr << " should be " << (r.valid? "valid" : "invalid");
      if (v)
      {
         EXPECT_EQ(r.address, n.address) << r.cidr << " address should be " << std::hex << r.address.s_addr;
         EXPECT_EQ(r.netmask, n.netmask) << r.cidr << " netmask should be " << std::hex << r.netmask.s_addr;
         EXPECT_EQ(r.prefix, n.prefix) << r.cidr << " prefix should be " << (int) r.prefix;
      }
   }
}

TEST_F(TestIPv4Network, matches24)
{
   IPv4Network net;
   struct in_addr ina;
   EXPECT_EQ(true, IPv4Network::parse("192.168.1.0/24", net));
   for (int i = 1; i <= 255; ++i)
   {
      std::string a = std::string("192.168.1.") + std::to_string(i);
      EXPECT_EQ(1, inet_pton(AF_INET, a.c_str(), &ina));
      EXPECT_EQ(true, net.matches(ina));
   }
   for (int i = 1; i <= 255; ++i)
   {
      std::string a = std::string("192.168.2.") + std::to_string(i);
      EXPECT_EQ(1, inet_pton(AF_INET, a.c_str(), &ina));
      EXPECT_EQ(false, net.matches(ina));
   }
}

TEST_F(TestIPv4Network, matches32)
{
   IPv4Network net;
   struct in_addr ina;
   EXPECT_EQ(true, IPv4Network::parse("192.168.1.128/32", net));
   for (int i = 1; i <= 255; ++i)
   {
      std::string a = std::string("192.168.1.") + std::to_string(i);
      EXPECT_EQ(1, inet_pton(AF_INET, a.c_str(), &ina));
      EXPECT_EQ(i == 128, net.matches(ina));
   }
}

TEST_F(TestIPv4Network, matches0)
{
   IPv4Network net;
   struct in_addr ina;
   EXPECT_EQ(true, IPv4Network::parse("0.0.0.0/0", net));
   for (int i = 1; i <= 255; ++i)
   {
      std::string a = std::string("192.168.1.") + std::to_string(i);
      EXPECT_EQ(1, inet_pton(AF_INET, a.c_str(), &ina));
      EXPECT_EQ(true, net.matches(ina));
   }
}
