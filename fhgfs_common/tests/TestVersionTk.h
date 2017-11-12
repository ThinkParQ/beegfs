#ifndef TESTVERSIONTK_H_
#define TESTVERSIONTK_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestVersionTk: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestVersionTk );
   CPPUNIT_TEST( testEncodeAndDecode );
   CPPUNIT_TEST( testComparison );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestVersionTk();
      virtual ~TestVersionTk();

      void setUp();
      void tearDown();

      void testEncodeAndDecode();
      void testComparison();
};


#endif /* TESTVERSIONTK_H_ */
