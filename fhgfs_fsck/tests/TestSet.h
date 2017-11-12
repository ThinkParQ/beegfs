#ifndef TESTSET_H_
#define TESTSET_H_

#include "FlatTest.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestSet : public CppUnit::TestFixture, protected FlatTest
{
   CPPUNIT_TEST_SUITE(TestSet);

   CPPUNIT_TEST(testOpen);
   CPPUNIT_TEST(testDrop);
   CPPUNIT_TEST(testClear);
   CPPUNIT_TEST(testMerge);
   CPPUNIT_TEST(testDataOps);

   CPPUNIT_TEST_SUITE_END();

   public:
      void setUp() { FlatTest::setUp(); }
      void tearDown() { FlatTest::tearDown(); }

   private:
      void testOpen();
      void testDrop();
      void testClear();
      void testMerge();
      void testDataOps();
};

#endif
