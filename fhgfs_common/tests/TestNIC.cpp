#include "TestNIC.h"

#include <common/net/sock/NetworkInterfaceCard.h>
#include <tests/TestRunnerBase.h>

REGISTER_TEST_SUITE(TestNIC);

void TestNIC::testNIC()
{
   NicAddressList list;
   StringList allowedInterfaces;

   CPPUNIT_ASSERT(NetworkInterfaceCard::findAll(&allowedInterfaces, true, true, &list));

   CPPUNIT_ASSERT(!list.empty());

   NicAddress nicAddr;

   CPPUNIT_ASSERT(NetworkInterfaceCard::findByName(list.begin()->name, &nicAddr));
}
