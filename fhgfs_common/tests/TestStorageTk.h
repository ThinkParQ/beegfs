#ifndef TEST_STORAGETK_H
#define TEST_STORAGETK_H

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestStorageTk : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestStorageTk);
   CPPUNIT_TEST(testFindMountedPrefix);
   CPPUNIT_TEST_SUITE_END();

   private:
      void testFindMountedPrefix();
};

#endif
