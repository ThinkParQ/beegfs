#ifndef TESTSERIALIZATION_H_
#define TESTSERIALIZATION_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cstring>

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>

class TestSerialization : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestSerialization);

   CPPUNIT_TEST(testByteswap);

   CPPUNIT_TEST(testIsDirection);

   CPPUNIT_TEST(testPrimitiveTypes);
   CPPUNIT_TEST(testPair);
   CPPUNIT_TEST(testTuple);
   CPPUNIT_TEST(testPadFieldTo);
   CPPUNIT_TEST(testSerializeAs);
   CPPUNIT_TEST(testRawString);
   CPPUNIT_TEST(testStr);
   CPPUNIT_TEST(testBackedPtr);
   CPPUNIT_TEST(testRawBlock);

   CPPUNIT_TEST(testBlock);

   CPPUNIT_TEST(testNonPrimitive);
   CPPUNIT_TEST(testCollections);
   CPPUNIT_TEST(testAtomic);
   CPPUNIT_TEST(testBase);
   CPPUNIT_TEST(testStringCollections);

   CPPUNIT_TEST_SUITE_END();

   private:
      void testByteswap();

      void testIsDirection();

      void testPrimitiveTypes();
      void testPair();
      void testTuple();
      void testPadFieldTo();
      void testSerializeAs();
      void testRawString();
      void testStr();
      void testBackedPtr();
      void testRawBlock();

      void testBlock();

      void testNonPrimitive();
      void testCollections();
      void testAtomic();
      void testBase();
      void testStringCollections();
};

#endif
