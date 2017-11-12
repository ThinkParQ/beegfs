#include "TestNetFilter.h"

#include <common/toolkit/NetFilter.h>
#include <tests/TestRunnerBase.h>

REGISTER_TEST_SUITE(TestNetFilter);

void TestNetFilter::createFile(const char* content)
{
   fileName = "/tmp/XXXXXX";
   int fd = mkstemp(&fileName[0]);
   CPPUNIT_ASSERT(fd >= 0);
   CPPUNIT_ASSERT((write(fd, content, strlen(content)) == ssize_t(strlen(content))));
   close(fd);
}

void TestNetFilter::removeFile()
{
   unlink(fileName.c_str());
}

void TestNetFilter::testNoFile()
{
   NetFilter filter("");

   CPPUNIT_ASSERT(filter.getNumFilterEntries() == 0);

   // always allow loopback
   CPPUNIT_ASSERT(filter.isAllowed(htonl(INADDR_LOOPBACK)));
   CPPUNIT_ASSERT(filter.isContained(htonl(INADDR_LOOPBACK)));

   // can't check for all ips, try a random one
   CPPUNIT_ASSERT(filter.isAllowed(htonl(0xd5c1a899)));
   CPPUNIT_ASSERT(!filter.isContained(htonl(0xd5c1a899)));
}

void TestNetFilter::testNetmask0()
{
   createFile("10.0.0.0/0");

   try
   {
      NetFilter filter(fileName);

      CPPUNIT_ASSERT(filter.getNumFilterEntries() == 1);

      // always allow loopback
      CPPUNIT_ASSERT(filter.isAllowed(htonl(INADDR_LOOPBACK)));
      CPPUNIT_ASSERT(filter.isContained(htonl(INADDR_LOOPBACK)));

      // 10.0.0.x should definitely be allowed
      CPPUNIT_ASSERT(filter.isAllowed(htonl(0x0a010203)));
      CPPUNIT_ASSERT(filter.isContained(htonl(0x0a010203)));

      // netfilter /0 says everything else is allowed too
      CPPUNIT_ASSERT(filter.isAllowed(htonl(0xd5c1a899)));
      CPPUNIT_ASSERT(filter.isContained(htonl(0xd5c1a899)));

      removeFile();
   }
   catch (...)
   {
      removeFile();
      throw;
   }
}

void TestNetFilter::testNetmaskExclusive()
{
   createFile("10.0.0.0/24\n10.1.0.0/24");

   try
   {
      NetFilter filter(fileName);

      CPPUNIT_ASSERT(filter.getNumFilterEntries() == 2);

      // always allow loopback
      CPPUNIT_ASSERT(filter.isAllowed(htonl(INADDR_LOOPBACK)));
      CPPUNIT_ASSERT(filter.isContained(htonl(INADDR_LOOPBACK)));

      // 10.0.0.x should definitely be allowed
      CPPUNIT_ASSERT(filter.isAllowed(htonl(0x0a000003)));
      CPPUNIT_ASSERT(filter.isContained(htonl(0x0a000003)));
      // 10.0.1.x should not be allowed
      CPPUNIT_ASSERT(!filter.isAllowed(htonl(0x0a000103)));
      CPPUNIT_ASSERT(!filter.isContained(htonl(0x0a000103)));

      // same thing for 10.1.0.x
      CPPUNIT_ASSERT(filter.isAllowed(htonl(0x0a010003)));
      CPPUNIT_ASSERT(filter.isContained(htonl(0x0a010003)));
      CPPUNIT_ASSERT(!filter.isAllowed(htonl(0x0a010103)));
      CPPUNIT_ASSERT(!filter.isContained(htonl(0x0a010103)));

      // other addresses are forbidden
      CPPUNIT_ASSERT(!filter.isAllowed(htonl(0xd5c1a899)));
      CPPUNIT_ASSERT(!filter.isContained(htonl(0xd5c1a899)));

      removeFile();
   }
   catch (...)
   {
      removeFile();
      throw;
   }
}
