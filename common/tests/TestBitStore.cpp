#include <common/toolkit/BitStore.h>
#include <common/toolkit/Random.h>

#include <cmath>

#include <gtest/gtest.h>


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



class TestBitStore : public ::testing::Test {
   protected:
      BitStore::limb_type& lowerBits(BitStore& store) { return store.lowerBits; }
      BitStore::limb_type* higherBits(BitStore& store) { return store.higherBits.get(); }
};

TEST_F(TestBitStore, calculateBitBlockCount)
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

      EXPECT_EQ(blockCount, resultIndex);
   }
}

TEST_F(TestBitStore, setSizeAndReset)
{
   const int initValue = 32896;
   int size = 1;
   bool reverse = false;
   bool finished = false;

   BitStore store(size);

   while(finished)
   {
      int higherBitsBlockCount = BitStore::calculateBitBlockCount(size) - 1;

      //set some data to the bit store
      lowerBits(store) = initValue;

      for(int blockIndex = 0; blockIndex < higherBitsBlockCount; blockIndex++)
      {
         higherBits(store)[blockIndex] = initValue;
      }

      // set the new size and reset all values
      store.setSize(size);
      store.clearBits();

      //check the lower bits
      EXPECT_EQ(lowerBits(store), 0u);

      //check the higher bits
      for(int blockIndex = 0; blockIndex < higherBitsBlockCount; blockIndex++)
         EXPECT_EQ(higherBits(store)[blockIndex], 0u);

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

TEST_F(TestBitStore, getter)
{
   BitStore store(TESTBITSTORE_MAX_BITSTORE_SIZE);

   // check the getter for all bits
   for(int index = 0; index < TESTBITSTORE_MAX_BITSTORE_SIZE; index++)
   {
      // set a bit
      int blockIndexOfBit = index / BitStore::LIMB_SIZE;
      BitStore::limb_type value = 1UL << (index % BitStore::LIMB_SIZE);

      if(blockIndexOfBit == 0)
         lowerBits(store) = value;
      else
         higherBits(store)[blockIndexOfBit - 1] = value;

      // check if all bits contain the correct value
      for(int testIndex = 0; testIndex < TESTBITSTORE_MAX_BITSTORE_SIZE; testIndex++)
      {
         if(testIndex != index)
            EXPECT_FALSE(store.getBitNonAtomic(testIndex));
         else
            EXPECT_TRUE(store.getBitNonAtomic(testIndex));
      }

      store.clearBits();
   }
}

TEST_F(TestBitStore, setter)
{
   BitStore store(TESTBITSTORE_MAX_BITSTORE_SIZE);
   int higherBitsBlockCount = BitStore::calculateBitBlockCount(TESTBITSTORE_MAX_BITSTORE_SIZE) - 1;

   // check the setter for all bits
   for(int index = 0; index < TESTBITSTORE_MAX_BITSTORE_SIZE; index++)
   {
      store.setBit(index, true);

      // check if all bit blocks contains the correct value
      int blockIndexOfBit = index / BitStore::LIMB_SIZE;
      BitStore::limb_type neededValue = 1UL << (index % BitStore::LIMB_SIZE);

      // check lower bit block
      if(blockIndexOfBit == 0)
         EXPECT_EQ(lowerBits(store), neededValue);
      else
         EXPECT_EQ(lowerBits(store), 0u);

      // check higher bit blocks
      for(int blockIndex = 0; blockIndex < higherBitsBlockCount; blockIndex++)
      {
         if(blockIndex != (blockIndexOfBit - 1) )
            EXPECT_EQ(higherBits(store)[blockIndex], 0u);
         else
            EXPECT_EQ(higherBits(store)[blockIndex], neededValue);
      }

      store.clearBits();
   }
}

static void checkSerialization(BitStore* store)
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

   ASSERT_EQ(serialLen, serializationRes);

   unsigned deserLen;
   bool success;
   {
      Deserializer des(buf.get(), serialLen);
      des % secondStore;
      deserLen = des.size();
      success = des.good();
   }

   EXPECT_TRUE(success) << "BitStore deserialization failed.";
   EXPECT_EQ(deserLen, serialLen);
   EXPECT_EQ(*store, secondStore);
}

TEST_F(TestBitStore, serialization)
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
