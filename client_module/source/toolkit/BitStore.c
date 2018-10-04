#include <common/toolkit/Serialization.h>
#include "BitStore.h"

/**
 * Set or unset a bit at the given index.
 *
 * Note: The bit operations in here are atomic.
 *
 * @param isSet true to set or false to unset the corresponding bit.
 */
void BitStore_setBit(BitStore* this, unsigned bitIndex, bool isSet)
{
   unsigned index = BitStore_getBitBlockIndex(bitIndex);
   unsigned indexInBitBlock = BitStore_getBitIndexInBitBlock(bitIndex);

   if(unlikely(bitIndex >= this->numBits) )
   {
      BEEGFS_BUG_ON(true, "index out of bounds: bitIndex >= this->numBits");
      return;
   }

   if (index == 0)
   {
      if (isSet)
         set_bit(indexInBitBlock, &this->lowerBits);
      else
         clear_bit(indexInBitBlock, &this->lowerBits);
   }
   else
   {
      if (isSet)
         set_bit(indexInBitBlock, &this->higherBits[index - 1]);
      else
         clear_bit(indexInBitBlock, &this->higherBits[index - 1]);
   }
}

/**
 * grow/shrink internal buffers when needed for new size.
 *
 * note: this method does not reset or copy bits, so consider the bits to be uninitialized after
 * calling this method. thus, you might want to call clearBits() afterwards to clear all bits.
 *
 * @param size number of bits
 */
void BitStore_setSize(BitStore* this, unsigned newSize)
{
   unsigned oldBitBlockCount = BitStore_calculateBitBlockCount(this->numBits);
   unsigned newBitBlockCount = BitStore_calculateBitBlockCount(newSize);

   if(newBitBlockCount != oldBitBlockCount)
   {
      SAFE_KFREE(this->higherBits);

      if(newBitBlockCount > 1)
      {
         this->higherBits = (bitstore_store_type*)os_kmalloc(
            BITSTORE_BLOCK_SIZE * (newBitBlockCount - 1) );
      }

      this->numBits = newBitBlockCount * BITSTORE_BLOCK_BIT_COUNT;
   }
}

/**
 * reset all bits to 0
 */
void BitStore_clearBits(BitStore* this)
{
   this->lowerBits = BITSTORE_BLOCK_INIT_MASK;

   if(this->higherBits)
   {
      unsigned bitBlockCount = BitStore_calculateBitBlockCount(this->numBits);

      memset(this->higherBits, 0, (bitBlockCount-1) * BITSTORE_BLOCK_SIZE);
   }

}


/**
 * note: serialized format is always one or more full 64bit blocks to keep the format independent
 * of 32bit/64bit archs.
 * note: serialized format is 8 byte aligned.
 */
void BitStore_serialize(SerializeCtx* ctx, const BitStore* this)
{
   unsigned index;
   unsigned blockCount = BitStore_calculateBitBlockCount(this->numBits);

   // size
   Serialization_serializeUInt(ctx, this->numBits);

   // padding for 8byte alignment
   Serialization_serializeUInt(ctx, 0);

   { // lowerBits
      if (BITSTORE_BLOCK_SIZE == sizeof(uint64_t) )
         Serialization_serializeUInt64(ctx, this->lowerBits);
      else
      if (BITSTORE_BLOCK_SIZE == sizeof(uint32_t) )
         Serialization_serializeUInt(ctx, this->lowerBits);
   }

   { // higherBits
      for(index = 0; index < (blockCount - 1); index++)
      {
         if (BITSTORE_BLOCK_SIZE == sizeof(uint64_t) )
            Serialization_serializeUInt64(ctx, this->higherBits[index]);
         else
         if (BITSTORE_BLOCK_SIZE == sizeof(uint32_t) )
            Serialization_serializeUInt(ctx, this->higherBits[index]);
      }
   }

   // add padding to allow 64bit deserialize from 32bit serialize
   if( (BITSTORE_BLOCK_SIZE == sizeof(uint32_t) ) && (blockCount & 1) )
   { // odd number of 32bit blocks
      Serialization_serializeUInt(ctx, 0);
   }
}

/**
 * @param outBitStoreStart deserialization buffer pointer for deserialize()
 */
bool BitStore_deserializePreprocess(DeserializeCtx* ctx, const char** outBitStoreStart)
{
   unsigned padding;
   unsigned tmpSize = 0; // =0, because it is used later and otherwise gcc complains
                         // with "may be uninitialized"
   unsigned blockCount;
   size_t blocksLen;

   *outBitStoreStart = ctx->data;

   // size
   if(unlikely(!Serialization_deserializeUInt(ctx, &tmpSize) ) )
      return false;

   // padding for 8byte alignment
   if(unlikely(!Serialization_deserializeUInt(ctx, &padding) ) )
      return false;

   // bit blocks

   blockCount = BitStore_calculateBitBlockCount(tmpSize);

   if(BITSTORE_BLOCK_SIZE == sizeof(uint64_t) )
      blocksLen = Serialization_serialLenUInt64() * blockCount;
   else
   if(BITSTORE_BLOCK_SIZE == sizeof(uint32_t) )
   {
      // add padding that allows 64bit deserialize from 32bit serialize
      if(blockCount & 1)
         blockCount++;

      blocksLen = Serialization_serialLenUInt() * blockCount;
   }
   else
      return false;

   // check buffer length
   if(unlikely(blocksLen > ctx->length) )
      return false;

   ctx->data += blocksLen;
   ctx->length -= blocksLen;

   return true;
}

void BitStore_deserialize(BitStore* this, DeserializeCtx* ctx)
{
   unsigned padding;
   unsigned blockCount;
   unsigned bitCount = 0;

   // size
   // store the bit count in a temp variable because setSizeAndReset() below needs old count
   Serialization_deserializeUInt(ctx, &bitCount);

   // padding for 8byte alignment
   Serialization_deserializeUInt(ctx, &padding);

   // clear and alloc memory for the higher bits if needed

   BitStore_setSize(this, bitCount);

   { // lowerBits
      if (BITSTORE_BLOCK_SIZE == sizeof(uint64_t) )
         Serialization_deserializeUInt64(ctx, (uint64_t*)&this->lowerBits);
      else
      if (BITSTORE_BLOCK_SIZE == sizeof(uint32_t) )
         Serialization_deserializeUInt(ctx, (uint32_t*)&this->lowerBits);
   }

   blockCount = BitStore_calculateBitBlockCount(this->numBits);

   { // higherBits
      unsigned index;

      BEEGFS_BUG_ON_DEBUG( (blockCount > 1) && !this->higherBits, "Bug: higherBits==NULL");

      for(index = 0; index < (blockCount - 1); index++)
      {
         bitstore_store_type* higherBits = &this->higherBits[index];

         if (BITSTORE_BLOCK_SIZE == sizeof(uint64_t) )
            Serialization_deserializeUInt64(ctx, (uint64_t*)higherBits);
         else
         if (BITSTORE_BLOCK_SIZE == sizeof(uint32_t) )
            Serialization_deserializeUInt(ctx, (uint32_t*)higherBits);
      }
   }

   // add padding that allows 64bit deserialize from 32bit serialize
   if( (BITSTORE_BLOCK_SIZE == sizeof(uint32_t) ) && (blockCount & 1) )
      Serialization_deserializeUInt(ctx, &padding);
}

/**
 * Update this store with values from given other store.
 */
void BitStore_copy(BitStore* this, BitStore* other)
{
   BitStore_setSize(this, other->numBits);

   this->lowerBits = other->lowerBits;

   if(!this->higherBits)
      return;

   // we have higherBits to copy
   {
      unsigned blockCount = BitStore_calculateBitBlockCount(this->numBits);

      memcpy(this->higherBits, other->higherBits, (blockCount-1) * BITSTORE_BLOCK_SIZE);
   }
}

/**
 * Update this store with values from given other store.
 *
 * To make this copy thread-safe, this method does not attempt any re-alloc of internal buffers,
 * so the copy will not be complete if the second store contains more blocks.
 */
void BitStore_copyThreadSafe(BitStore* this, const BitStore* other)
{
   unsigned thisBlockCount;
   unsigned otherBlockCount;

   this->lowerBits = other->lowerBits;

   if(!this->higherBits)
      return;

   // copy (or clear) our higherBits

   thisBlockCount = BitStore_calculateBitBlockCount(this->numBits);
   otherBlockCount = BitStore_calculateBitBlockCount(other->numBits);

   if(otherBlockCount > 1)
   { // copy as much as we can from other's higherBits
      unsigned minCommonBlockCount = MIN(thisBlockCount, otherBlockCount);

      memcpy(this->higherBits, other->higherBits, (minCommonBlockCount-1) * BITSTORE_BLOCK_SIZE);
   }

   // (this store must at least have one higherBits block, because higherBits!=NULL)
   if(thisBlockCount > otherBlockCount)
   { // zero remaining higherBits of this store
      unsigned numBlocksDiff = thisBlockCount - otherBlockCount;

      // ("otherBlockCount-1": -1 for lowerBits block)
      memset(&(this->higherBits[otherBlockCount-1]), 0, numBlocksDiff * BITSTORE_BLOCK_SIZE);
   }
}
