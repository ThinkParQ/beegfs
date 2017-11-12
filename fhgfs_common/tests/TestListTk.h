#ifndef TESTLISTTK_H_
#define TESTLISTTK_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestListTk: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestListTk );
   CPPUNIT_TEST( testAdvance );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestListTk();
      virtual ~TestListTk();

      void setUp();
      void tearDown();

      void testAdvance();
};

#endif /* TESTLISTTK_H_ */
