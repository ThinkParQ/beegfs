#ifndef TESTCOMMUNICATION_H
#define TESTCOMMUNICATION_H

#include <common/app/log/LogContext.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestCommunication: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestCommunication );
   CPPUNIT_TEST( testRequestResponse );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestCommunication();
      virtual ~TestCommunication();

      void setUp();
      void tearDown();

      void testRequestResponse();

   private:
      LogContext log;
};

#endif /* TESTCOMMUNICATION_H */
