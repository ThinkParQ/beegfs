#include <common/net/sock/Socket.cpp>

#include <gtest/gtest.h>


class TestSocket : public ::testing::Test
{
};

TEST_F(TestSocket, endpointAddrToStrSockaddr)
{
   std::string addr = "10.20.30.40";
   short port = 1234;
   struct sockaddr_in sin;

   sin.sin_port = htons(port);
   EXPECT_EQ(1, inet_pton(AF_INET, addr.c_str(), &sin.sin_addr));
   EXPECT_EQ(addr + ":" + std::to_string(port), Socket::endpointAddrToStr(&sin));
}

