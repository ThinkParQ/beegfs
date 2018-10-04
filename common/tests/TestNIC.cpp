#include <common/net/sock/NetworkInterfaceCard.h>

#include <gtest/gtest.h>

TEST(NIC, nic)
{
   NicAddressList list;
   StringList allowedInterfaces;

   ASSERT_TRUE(NetworkInterfaceCard::findAll(&allowedInterfaces, true, &list));

   ASSERT_FALSE(list.empty());

   NicAddress nicAddr;

   ASSERT_TRUE(NetworkInterfaceCard::findByName(list.begin()->name, &nicAddr));
}
