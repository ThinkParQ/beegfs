#include <common/toolkit/serialization/Serialization.h>
#include <common/app/log/LogContext.h>
#include "BitStore.h"



/**
 * Set or unset a bit at the given index.
 *
 * Note: The bit operations in here are not atomic.
 *
 * @param isSet true to set or false to unset the corresponding bit.
 */
void BitStore::setBit(unsigned bitIndex, bool isSet)
{
   unsigned index = bitIndex / LIMB_SIZE;
   unsigned indexInBitBlock = bitIndex % LIMB_SIZE;
   limb_type mask = 1UL << indexInBitBlock;

   if(unlikely(bitIndex >= this->numBits) )
      return;

   if (index == 0)
   {
      if (isSet)
         this->lowerBits = this->lowerBits | mask;
      else
         this->lowerBits = this->lowerBits & (~mask);
   }
   else
   {
      if (isSet)
         this->higherBits[index - 1] = this->higherBits[index - 1] | mask;
      else
         this->higherBits[index - 1] = this->higherBits[index - 1] & (~mask);
   }
}

/**
 * grow/shrink internal buffers as needed for new size.
 *
 * note: this method does not reset or copy bits, so consider the bits to be uninitialized after
 * calling this method. thus, you might want to call clearBits() afterwards to clear all bits.
 *
 * @param size number of bits
 */
void BitStore::setSize(unsigned newSize)
{
   unsigned oldBitBlockCount = calculateBitBlockCount(this->numBits);
   unsigned newBitBlockCount = calculateBitBlockCount(newSize);

   if(newBitBlockCount != oldBitBlockCount)
   {
      higherBits.reset();

      if(newBitBlockCount > 1)
         higherBits.reset(new limb_type[newBitBlockCount - 1]);

      this->numBits = newBitBlockCount * LIMB_SIZE;
   }
}

/**
 * reset all bits to 0.
 */
void BitStore::clearBits()
{
   this->lowerBits = 0;

   if(this->higherBits)
   {
      unsigned bitBlockCount = calculateBitBlockCount(this->numBits);

      memset(higherBits.get(), 0, (bitBlockCount-1) * sizeof(limb_type));
   }
}

/**
 * release the memory of the higher bits and set the size of the bit store to the minimal value
 */
void BitStore::freeHigherBits()
{
   higherBits.reset();
   this->numBits = LIMB_SIZE;
}

/**
 * note: serialized format is 8 byte aligned.
 */
void BitStore::serialize(Serializer& ser) const
{
   ser
      % numBits
      % uint32_t(0) // PADDING
      % lowerBits;

   unsigned blockCount = calculateBitBlockCount(this->numBits);

   for(unsigned i = 0; i < (blockCount - 1); i++)
      ser % higherBits[i];

   // PADDING to 64bit
   if(blockCount & 1)
      ser % uint32_t(0);
}

void BitStore::serialize(Deserializer& des)
{
   uint32_t numBits;
   uint32_t dummy;

   des
      % numBits
      % dummy // PADDING
      % lowerBits;

   setSize(numBits);

   unsigned blockCount = calculateBitBlockCount(this->numBits);
   for(unsigned i = 0; i < (blockCount - 1); i++)
      des % higherBits[i];

   // PADDING to 64bit
   if(blockCount & 1)
      des % dummy;
}

/**
 * compare two BitStores.
 *
 * @return true if both BitStores are equal
 */
bool BitStore::operator==(const BitStore& other) const
{
   if(numBits != other.numBits
         || lowerBits != other.lowerBits)
      return false;

   unsigned blockCount = calculateBitBlockCount(numBits);

   return std::equal(higherBits.get(), higherBits.get() + blockCount - 1, other.higherBits.get());
}

/**
 * Update this store with values from given other store.
 */
BitStore& BitStore::operator=(const BitStore& other)
{
   setSize(other.numBits);

   this->lowerBits = other.lowerBits;

   if(!this->higherBits)
      return *this;

   // we have higherBits to copy

   unsigned blockCount = calculateBitBlockCount(this->numBits);

   memcpy(higherBits.get(), other.higherBits.get(), (blockCount-1) * sizeof(limb_type));
   return *this;
}
