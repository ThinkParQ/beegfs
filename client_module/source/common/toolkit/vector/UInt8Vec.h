#ifndef UINT8VEC_H_
#define UINT8VEC_H_

#include <common/toolkit/list/UInt8List.h>


struct UInt8Vec;
typedef struct UInt8Vec UInt8Vec;

static inline void UInt8Vec_init(UInt8Vec* this);
static inline void UInt8Vec_uninit(UInt8Vec* this);
static inline void UInt8Vec_append(UInt8Vec* this, uint8_t value);
static inline size_t UInt8Vec_length(UInt8Vec* this);
static inline void UInt8Vec_clear(UInt8Vec* this);

// getters & setters
static inline uint8_t UInt8Vec_at(UInt8Vec* this, size_t index);


/**
 * Note: Derived from the corresponding list. Use the list iterator for read-only access
 */
struct UInt8Vec
{
   UInt8List UInt8List;

   uint8_t* vecArray;
   size_t vecArrayLen;
};


void UInt8Vec_init(UInt8Vec* this)
{
   UInt8List_init( (UInt8List*)this);

   this->vecArrayLen = 4;
   this->vecArray = (uint8_t*)os_kmalloc(this->vecArrayLen * sizeof(uint8_t) );
}

void UInt8Vec_uninit(UInt8Vec* this)
{
   kfree(this->vecArray);

   UInt8List_uninit( (UInt8List*)this);
}

void UInt8Vec_append(UInt8Vec* this, uint8_t value)
{
   size_t newListLen;

   UInt8List_append( (UInt8List*)this, value);

   newListLen = UInt8List_length( (UInt8List*)this);

   // check if we have enough buffer space for new elem

   if(newListLen > this->vecArrayLen)
   { // double vector array size: alloc new, copy values, delete old, switch to new
      uint8_t* newVecArray = (uint8_t*)os_kmalloc(this->vecArrayLen * sizeof(uint8_t) * 2);
      memcpy(newVecArray, this->vecArray, this->vecArrayLen * sizeof(uint8_t) );

      kfree(this->vecArray);

      this->vecArrayLen = this->vecArrayLen * 2;
      this->vecArray = newVecArray;
   }

   // add value to last array elem (determine last used index based on list length)

   (this->vecArray)[newListLen-1] = value;
}

size_t UInt8Vec_length(UInt8Vec* this)
{
   return UInt8List_length( (UInt8List*)this);
}

uint8_t UInt8Vec_at(UInt8Vec* this, size_t index)
{
   BEEGFS_BUG_ON_DEBUG(index >= UInt8Vec_length(this), "Index out of bounds");

   return this->vecArray[index];
}

void UInt8Vec_clear(UInt8Vec* this)
{
   UInt8List_clear( (UInt8List*)this);
}



#endif /* UINT8VEC_H_ */
