#ifndef TESTDATABASE_H_
#define TESTDATABASE_H_

#include <common/app/log/LogContext.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#define DB_TEST_FILE "/tmp/beegfs_admon/testing/db/test.db"

class TestDatabase: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestDatabase );
   CPPUNIT_TEST( testNewDBCreation );
   CPPUNIT_TEST( testExistingDBOpen );
   CPPUNIT_TEST( testBrokenDBOpen );
   CPPUNIT_TEST( testClearBrokenDBOpen );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestDatabase();
      virtual ~TestDatabase();

      void setUp();
      void tearDown();

      void testNewDBCreation();
      void testExistingDBOpen();
      void testBrokenDBOpen();
      void testClearBrokenDBOpen();

   private:
      LogContext log;
      std::string testDbFile;
};

#endif /* TESTDATABASE_H_ */
