#include <common/toolkit/BitStore.h>
#include <common/toolkit/Random.h>

#include <math.h>
#include "TestBitStore.h"

TestBitStore::TestBitStore()
{
}

TestBitStore::~TestBitStore()
{
}

/*
 * will be executed before every test
 * nothing to do
 */
void TestBitStore::setUp()
{
}

/*
 * will be executed after every test
 * nothing to do
 */
void TestBitStore::tearDown()
{
}


void TestBitStore::testCalculateBitBlockCount()
{
   unsigned blockCount;
   unsigned resultIndex = 0;

   for(int size = 1; size < TESTBITSTORE_MAX_BITSTORE_SIZE; size++)
   {
      if(size < 33)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[0].blockCount;
      else
      if(size < 65)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[1].blockCount;
      else
      if(size < 97)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[2].blockCount;
      else
      if(size < 129)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[3].blockCount;
      else
      if(size < 161)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[4].blockCount;
      else
      if(size < 193)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[5].blockCount;
      else
      if(size < 225)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[6].blockCount;
      else
      if(size < 257)
         resultIndex = __TESTBITSTORE_BLOCK_COUNT_RESULTS[7].blockCount;

      blockCount = BitStore::calculateBitBlockCount(size);

      CPPUNIT_ASSERT_MESSAGE("", blockCount == resultIndex);
   }
}

void TestBitStore::testSetSizeAndReset()
{
   const int initValue = 32896;
   int size = 1;
   bool reverse = false;
   bool finished = false;

   BitStore store(size);

   while(finished)
   {
      int higherBitsBlockCount = store.calculateBitBlockCount(size) - 1;

      //set some data to the bit store
      store.lowerBits = initValue;

      for(int blockIndex = 0; blockIndex < higherBitsBlockCount; blockIndex++)
      {
         store.higherBits[blockIndex] = initValue;
      }

      // set the new size and reset all values
      store.setSize(size);
      store.clearBits();

      //check the lower bits
      CPPUNIT_ASSERT_MESSAGE("A bit is set which must be zero.", store.lowerBits == 0);

      //check the higher bits
      for(int blockIndex = 0; blockIndex < higherBitsBlockCount; blockIndex++)
      {
         CPPUNIT_ASSERT_MESSAGE("A bit is set which must be zero.",
            store.higherBits[blockIndex] == 0);
      }

      if(size == TESTBITSTORE_MAX_BITSTORE_SIZE)
         reverse = true;

      if(reverse)
         size--;
      else
         size++;

      //

      // finish test or set the new size
      if(reverse && size == 0)
         finished = true;
   }
}

void TestBitStore::testGetter()
{
   BitStore store(TESTBITSTORE_MAX_BITSTORE_SIZE);

   // check the getter for all bits
   for(int index = 0; index < TESTBITSTORE_MAX_BITSTORE_SIZE; index++)
   {
      // set a bit
      int blockIndexOfBit = index / BitStore::LIMB_SIZE;
      BitStore::limb_type value = 1UL << (index % BitStore::LIMB_SIZE);

      if(blockIndexOfBit == 0)
         store.lowerBits = value;
      else
         store.higherBits[blockIndexOfBit - 1] = value;

      // check if all bits contain the correct value
      for(int testIndex = 0; testIndex < TESTBITSTORE_MAX_BITSTORE_SIZE; testIndex++)
      {
         if(testIndex != index)
            CPPUNIT_ASSERT_MESSAGE("Another bit is set which should be zero.",
               !store.getBitNonAtomic(testIndex) );
         else
            CPPUNIT_ASSERT_MESSAGE("Bit should be set, but is cleared.",
               store.getBitNonAtomic(testIndex) );
      }

      store.clearBits();
   }
}

void TestBitStore::testSetter()
{
   BitStore store(TESTBITSTORE_MAX_BITSTORE_SIZE);
   int higherBitsBlockCount = store.calculateBitBlockCount(TESTBITSTORE_MAX_BITSTORE_SIZE) - 1;

   // check the setter for all bits
   for(int index = 0; index < TESTBITSTORE_MAX_BITSTORE_SIZE; index++)
   {
      store.setBit(index, true);

      // check if all bit blocks contains the correct value
      int blockIndexOfBit = index / BitStore::LIMB_SIZE;
      BitStore::limb_type neededValue = 1UL << (index % BitStore::LIMB_SIZE);

      // check lower bit block
      if(blockIndexOfBit == 0)
         CPPUNIT_ASSERT_MESSAGE("The bit isn't set correctly.", store.lowerBits == neededValue);
      else
         CPPUNIT_ASSERT_MESSAGE("A other bit is set which must be zero.", store.lowerBits == 0);

      // check higher bit blocks
      for(int blockIndex = 0; blockIndex < higherBitsBlockCount; blockIndex++)
      {
         if(blockIndex != (blockIndexOfBit - 1) )
            CPPUNIT_ASSERT_MESSAGE("A other bit is set which must be zero.",
               store.higherBits[blockIndex] == 0);
         else
            CPPUNIT_ASSERT_MESSAGE("The bit isn't set correctly.",
               store.higherBits[blockIndex] == neededValue);
      }

      store.clearBits();
   }
}

void TestBitStore::testSerialization()
{
   // check serialization for different BitStore size
   for(int size = 1; size <= TESTBITSTORE_MAX_BITSTORE_SIZE; size++)
   {
      BitStore store(size);

      // check serialization for all bits, only one bit is set
      for(int index = 0; index < size; index++)
      {
         store.setBit(index, true);

         checkSerialization(&store);
         store.clearBits();
      }
   }

   // check serialization for BitStore with random values
   BitStore store(TESTBITSTORE_MAX_BITSTORE_SIZE);
   Random rand;

   for(int count = 0; count < TESTBITSTORE_RANDOM_VALUES_COUNT; count++)
   {
      int randIndex = rand.getNextInRange(0, TESTBITSTORE_MAX_BITSTORE_SIZE - 1);
      store.setBit(randIndex, true);
   }

   checkSerialization(&store);
}

/**
 * Serialize and deserialize a BitStore and compare results.
 */
void TestBitStore::checkSerialization(BitStore* store)
{
   BitStore secondStore;

   unsigned serialLen;
   {
      Serializer ser;
      ser % *store;
      serialLen = ser.size();
   }

   boost::scoped_array<char> buf(new char[serialLen]);

   unsigned serializationRes;
   {
      Serializer ser(buf.get(), serialLen);
      ser % *store;
      serializationRes = ser.size();
   }

   CPPUNIT_ASSERT_MESSAGE("BitStore serialLen and serializationRes don't match.",
      serialLen == serializationRes);

   unsigned deserLen;
   bool success;
   {
      Deserializer des(buf.get(), serialLen);
      des % secondStore;
      deserLen = des.size();
      success = des.good();
   }

   CPPUNIT_ASSERT_MESSAGE("BitStore deserialization failed.", success);

   CPPUNIT_ASSERT_MESSAGE("Bad BitStore deserialization length.",
      deserLen == serialLen);

   CPPUNIT_ASSERT_MESSAGE("BitStores are not equals after deserialization.",
      *store == secondStore);
}
