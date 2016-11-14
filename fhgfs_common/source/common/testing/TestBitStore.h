#ifndef TESTBITSTORE_H_
#define TESTBITSTORE_H_

#include <common/app/log/LogContext.h>
#include <common/toolkit/BitStore.h>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>


#define TESTBITSTORE_MAX_BITSTORE_SIZE                242
#define TESTBITSTORE_RANDOM_VALUES_COUNT              75

struct TestBitStoreBlockCountResult
{
   int blockCount;    // the needed blockCount
};

/**
 * The number of blocks required to store a certain number of bits.
 */
struct TestBitStoreBlockCountResult const __TESTBITSTORE_BLOCK_COUNT_RESULTS[] =
{
   {1 + ( ( (32*1)-1) / BitStore::LIMB_SIZE) },        // result for bits 1 to 32
   {1 + ( ( (32*2)-1) / BitStore::LIMB_SIZE) },        // result for bits 33 to 64
   {1 + ( ( (32*3)-1) / BitStore::LIMB_SIZE) },        // result for bits 65 to 96
   {1 + ( ( (32*4)-1) / BitStore::LIMB_SIZE) },        // result for bits 97 to 128
   {1 + ( ( (32*5)-1) / BitStore::LIMB_SIZE) },        // result for bits 129 to 160
   {1 + ( ( (32*6)-1) / BitStore::LIMB_SIZE) },        // result for bits 161 to 192
   {1 + ( ( (32*7)-1) / BitStore::LIMB_SIZE) },        // result for bits 193 to 224
   {1 + ( ( (32*8)-1) / BitStore::LIMB_SIZE) },        // result for bits 125 to 256
};


class TestBitStore : public CppUnit::TestFixture
{
      CPPUNIT_TEST_SUITE( TestBitStore );
      CPPUNIT_TEST( testCalculateBitBlockCount );
      CPPUNIT_TEST( testSetSizeAndReset );
      CPPUNIT_TEST( testGetter );
      CPPUNIT_TEST( testSetter );
      CPPUNIT_TEST( testSerialization );
      CPPUNIT_TEST_SUITE_END();

   public:
      TestBitStore();
      virtual ~TestBitStore();

      void setUp();
      void tearDown();

   private:
      void testCalculateBitBlockCount();
      void testSetSizeAndReset();
      void testGetter();
      void testSetter();
      void testSerialization();

      void checkSerialization(BitStore* store);
};

#endif /* TESTBITSTORE_H_ */
