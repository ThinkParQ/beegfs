#ifndef BITSTORE_COMMON_H_
#define BITSTORE_COMMON_H_


#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>


/**
 * A vector of bits.
 */
class BitStore
{
   friend class TestBitStore;

   public:
      typedef uint32_t limb_type;

      static const unsigned int LIMB_SIZE = 32;

   public:
      BitStore()
      {
         init(true);
      }

      explicit BitStore(int size)
      {
         init(false);
         setSize(size);
         clearBits();
      }

      BitStore(const BitStore& other)
      {
         init(true);
         *this = other;
      }

      void setBit(unsigned bitIndex, bool isSet);

      void setSize(unsigned newSize);
      void clearBits();

      bool operator==(const BitStore& other) const;

      bool operator!=(const BitStore& other) const { return !(*this == other); }

      BitStore& operator=(const BitStore& other);

      void serialize(Serializer& ser) const;
      void serialize(Deserializer& des);

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         obj->serialize(ctx);
      }

      static unsigned calculateBitBlockCount(unsigned size)
      {
         return size < LIMB_SIZE
            ? 1
            : (size + LIMB_SIZE - 1) / LIMB_SIZE;
      }


   private:
      uint32_t numBits; // max number of bits that this store can hold
      limb_type lowerBits; // used to avoid overhead of extra alloc for higherBits
      boost::scoped_array<limb_type> higherBits; // array, which is alloc'ed only when needed

      void freeHigherBits();


   protected:

   public:
      // inliners

      void init(bool setZero)
      {
         this->numBits = LIMB_SIZE;
         this->higherBits.reset();

         if (setZero)
            this->lowerBits = 0;
      }

      /**
       * Test whether the bit at the given index is set or not.
       *
       * Note: The bit operations in here are non-atomic.
       */
      bool getBitNonAtomic(unsigned bitIndex) const
      {
         size_t index;
         size_t indexInBitBlock;
         limb_type mask;

         if (bitIndex >= this->numBits)
            return false;

         index = bitIndex / LIMB_SIZE;
         indexInBitBlock = bitIndex % LIMB_SIZE;
         mask = 1UL << indexInBitBlock;

         if (index == 0)
            return (this->lowerBits & mask) ? true : false;
         else
            return (this->higherBits[index - 1] & mask) ? true : false;
      }
};

#endif /* BITSTORE_COMMON_H_ */
