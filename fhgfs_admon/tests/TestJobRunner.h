#ifndef TESTJOBRUNNER_H_
#define TESTJOBRUNNER_H_

#include <common/app/log/LogContext.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#define TEST_FILE "/tmp/beegfs_admon/testing/jobrunner/test.db"

class TestJobRunner: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestJobRunner );
   CPPUNIT_TEST( testTouch );
   CPPUNIT_TEST( testUnknownCmd );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestJobRunner();
      virtual ~TestJobRunner();

      void setUp();
      void tearDown();

      void testTouch();
      void testUnknownCmd();

   private:
      LogContext log;
};

#endif /* TESTJOBRUNNER_H_ */
