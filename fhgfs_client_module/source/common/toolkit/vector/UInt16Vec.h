#ifndef UINT16VEC_H_
#define UINT16VEC_H_

#include <common/toolkit/list/UInt16List.h>


struct UInt16Vec;
typedef struct UInt16Vec UInt16Vec;

static inline void UInt16Vec_init(UInt16Vec* this);
static inline void UInt16Vec_uninit(UInt16Vec* this);
static inline void UInt16Vec_append(UInt16Vec* this, uint16_t value);
static inline size_t UInt16Vec_length(UInt16Vec* this);
static inline void UInt16Vec_clear(UInt16Vec* this);

// getters & setters
static inline uint16_t UInt16Vec_at(UInt16Vec* this, size_t index);


/**
 * Note: Derived from the corresponding list. Use the list iterator for read-only access
 */
struct UInt16Vec
{
   UInt16List UInt16List;

   uint16_t* vecArray;
   size_t vecArrayLen;
};


void UInt16Vec_init(UInt16Vec* this)
{
   UInt16List_init( (UInt16List*)this);

   this->vecArrayLen = 4;
   this->vecArray = (uint16_t*)os_kmalloc(this->vecArrayLen * sizeof(uint16_t) );
}

void UInt16Vec_uninit(UInt16Vec* this)
{
   kfree(this->vecArray);

   UInt16List_uninit( (UInt16List*)this);
}

void UInt16Vec_append(UInt16Vec* this, uint16_t value)
{
   size_t newListLen;

   UInt16List_append( (UInt16List*)this, value);

   newListLen = UInt16List_length( (UInt16List*)this);

   // check if we have enough buffer space for new elem

   if(newListLen > this->vecArrayLen)
   { // double vector array size: alloc new, copy values, delete old, switch to new
      uint16_t* newVecArray = (uint16_t*)os_kmalloc(this->vecArrayLen * sizeof(uint16_t) * 2);
      memcpy(newVecArray, this->vecArray, this->vecArrayLen * sizeof(uint16_t) );

      kfree(this->vecArray);

      this->vecArrayLen = this->vecArrayLen * 2;
      this->vecArray = newVecArray;
   }

   // add value to last array elem (determine last used index based on list length)

   (this->vecArray)[newListLen-1] = value;
}

size_t UInt16Vec_length(UInt16Vec* this)
{
   return UInt16List_length( (UInt16List*)this);
}

uint16_t UInt16Vec_at(UInt16Vec* this, size_t index)
{
   BEEGFS_BUG_ON_DEBUG(index >= UInt16Vec_length(this), "Index out of bounds");

   return this->vecArray[index];
}

void UInt16Vec_clear(UInt16Vec* this)
{
   UInt16List_clear( (UInt16List*)this);
}

#endif /* UINT16VEC_H_ */
