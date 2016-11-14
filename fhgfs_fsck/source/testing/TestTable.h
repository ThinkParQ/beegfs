#ifndef TESTTABLE_H_
#define TESTTABLE_H_

#include <database/Table.h>
#include <testing/FlatTest.h>

#include <boost/scoped_ptr.hpp>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestTable : public CppUnit::TestFixture, protected FlatTest
{
   CPPUNIT_TEST_SUITE(TestTable);

   CPPUNIT_TEST(testDataHandling);
   CPPUNIT_TEST(testBulkInsert);

   CPPUNIT_TEST_SUITE_END();

   public:
      void setUp();
      void tearDown();

   private:
      boost::scoped_ptr<Table<Data> > table;

      void testDataHandling();
      void testBulkInsert();
};

#endif
