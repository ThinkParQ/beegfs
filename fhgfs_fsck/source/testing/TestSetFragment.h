#ifndef TESTSETFRAGMENT_H_
#define TESTSETFRAGMENT_H_

#include <testing/FlatTest.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestSetFragment : public CppUnit::TestFixture, protected FlatTest
{
   CPPUNIT_TEST_SUITE(TestSetFragment);

   CPPUNIT_TEST(testOpen);
   CPPUNIT_TEST(testDrop);
   CPPUNIT_TEST(testFlush);
   CPPUNIT_TEST(testAppendAndAccess);
   CPPUNIT_TEST(testSortAndGet);
   CPPUNIT_TEST(testRename);

   CPPUNIT_TEST_SUITE_END();

   public:
      void setUp() { FlatTest::setUp(); }
      void tearDown() { FlatTest::tearDown(); }

   private:
      void testOpen();
      void testDrop();
      void testFlush();
      void testAppendAndAccess();
      void testSortAndGet();
      void testRename();
};

#endif
