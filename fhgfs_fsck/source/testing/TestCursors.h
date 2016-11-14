#ifndef TESTCURSORS_H_
#define TESTCURSORS_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestCursors : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestCursors);

   CPPUNIT_TEST(testVectorSource);
   CPPUNIT_TEST(testUnion);
   CPPUNIT_TEST(testSelect);
   CPPUNIT_TEST(testLeftJoinEq);
   CPPUNIT_TEST(testGroup);
   CPPUNIT_TEST(testFilter);
   CPPUNIT_TEST(testDistinct);

   CPPUNIT_TEST_SUITE_END();

   private:
      void testVectorSource();
      void testUnion();
      void testSelect();
      void testLeftJoinEq();
      void testGroup();
      void testFilter();
      void testDistinct();
};

#endif /* TESTDATABASE_H_ */
