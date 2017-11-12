#ifndef TEST_NETFILTER_H
#define TEST_NETFILTER_H

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestNetFilter : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestNetFilter);
   CPPUNIT_TEST(testNoFile);
   CPPUNIT_TEST(testNetmask0);
   CPPUNIT_TEST(testNetmaskExclusive);
   CPPUNIT_TEST_SUITE_END();

   private:
      void createFile(const char* content);
      void removeFile();

      void testNoFile();
      void testNetmask0();
      void testNetmaskExclusive();

      std::string fileName;
};

#endif
