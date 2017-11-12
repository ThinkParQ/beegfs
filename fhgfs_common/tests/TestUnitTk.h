#ifndef TESTUNITTK_H_
#define TESTUNITTK_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestUnitTk: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestUnitTk );
   CPPUNIT_TEST( testGigabyteToByte );
   CPPUNIT_TEST( testMegabyteToByte );
   CPPUNIT_TEST( testKilobyteToByte );
   CPPUNIT_TEST( testByteToXbyte );
   CPPUNIT_TEST( testXbyteToByte );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestUnitTk();
      virtual ~TestUnitTk();

      void setUp();
      void tearDown();

      void testGigabyteToByte();
      void testMegabyteToByte();
      void testKilobyteToByte();
      void testByteToXbyte();
      void testXbyteToByte();
};

#endif /* TESTUNITTK_H_ */
