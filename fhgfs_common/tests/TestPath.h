#ifndef TEST_PATH_H
#define TEST_PATH_H

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestPath : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestPath);
   CPPUNIT_TEST(testPath);
   CPPUNIT_TEST_SUITE_END();

   private:
      void testPath();
};

#endif
