#ifndef TESTFSCKTK_H_
#define TESTFSCKTK_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestFsckTk: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestFsckTk );

   CPPUNIT_TEST( testDirEntryTypeConversion );

   CPPUNIT_TEST_SUITE_END();

   public:
      TestFsckTk();
      virtual ~TestFsckTk();

      void setUp();
      void tearDown();

      void testDirEntryTypeConversion();
};

#endif /* TESTFSCKTK_H_ */
