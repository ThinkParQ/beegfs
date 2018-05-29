#ifndef INT64CPYVEC_H_
#define INT64CPYVEC_H_

#include <common/toolkit/list/Int64CpyList.h>

/**
 * Note: Derived from the corresponding list. Use the list iterator for read-only access
 */

struct Int64CpyVec;
typedef struct Int64CpyVec Int64CpyVec;

static inline void Int64CpyVec_init(Int64CpyVec* this);
static inline void Int64CpyVec_uninit(Int64CpyVec* this);
static inline void Int64CpyVec_append(Int64CpyVec* this, int64_t value);
static inline size_t Int64CpyVec_length(Int64CpyVec* this);
static inline void Int64CpyVec_clear(Int64CpyVec* this);

// getters & setters
static inline int64_t Int64CpyVec_at(Int64CpyVec* this, size_t index);


struct Int64CpyVec
{
   Int64CpyList Int64CpyList;

   int64_t** vecArray;
   size_t vecArrayLen;
};


void Int64CpyVec_init(Int64CpyVec* this)
{
   Int64CpyList_init( (Int64CpyList*)this);

   this->vecArrayLen = 4;
   this->vecArray = (int64_t**)os_kmalloc(
      this->vecArrayLen * sizeof(int64_t*) );
}

void Int64CpyVec_uninit(Int64CpyVec* this)
{
   kfree(this->vecArray);

   Int64CpyList_uninit( (Int64CpyList*)this);
}

void Int64CpyVec_append(Int64CpyVec* this, int64_t value)
{
   PointerListElem* lastElem;
   int64_t* lastElemValuePointer;

   Int64CpyList_append( (Int64CpyList*)this, value);

   // check if we have enough buffer space for new elem
   if(Int64CpyList_length( (Int64CpyList*)this) > this->vecArrayLen)
   { // double vector array size (create new, copy, exchange, delete old)
      int64_t** newVecArray =
         (int64_t**)os_kmalloc(this->vecArrayLen*sizeof(int64_t*)*2);
      memcpy(newVecArray, this->vecArray, this->vecArrayLen*sizeof(int64_t*) );
      kfree(this->vecArray);
      this->vecArrayLen = this->vecArrayLen * 2;
      this->vecArray = newVecArray;
   }

   // get last elem and add the valuePointer to the array
   lastElem = PointerList_getTail( (PointerList*)this);
   lastElemValuePointer = (int64_t*)lastElem->valuePointer;
   (this->vecArray)[Int64CpyList_length( (Int64CpyList*)this)-1] = lastElemValuePointer;
}

size_t Int64CpyVec_length(Int64CpyVec* this)
{
   return Int64CpyList_length( (Int64CpyList*)this);
}

int64_t Int64CpyVec_at(Int64CpyVec* this, size_t index)
{
   BEEGFS_BUG_ON_DEBUG(index >= Int64CpyVec_length(this), "Index out of bounds");

   return *(this->vecArray[index]);
}

void Int64CpyVec_clear(Int64CpyVec* this)
{
   Int64CpyList_clear( (Int64CpyList*)this);
}

#endif /*INT64CPYVEC_H_*/
