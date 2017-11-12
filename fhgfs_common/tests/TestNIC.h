#ifndef TEST_NIC_H
#define TEST_NIC_H

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestNIC : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestNIC);
   CPPUNIT_TEST(testNIC);
   CPPUNIT_TEST_SUITE_END();

   private:
      void testNIC();
};

#endif
