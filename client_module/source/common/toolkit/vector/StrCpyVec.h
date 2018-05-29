#ifndef STRCPYVEC_H_
#define STRCPYVEC_H_

#include <common/toolkit/list/StrCpyList.h>

/**
 * Note: Derived from the corresponding list. Use the list iterator for read-only access
 */

struct StrCpyVec;
typedef struct StrCpyVec StrCpyVec;

static inline void StrCpyVec_init(StrCpyVec* this);
static inline void StrCpyVec_uninit(StrCpyVec* this);
static inline void StrCpyVec_append(StrCpyVec* this, const char* valuePointer);
static inline size_t StrCpyVec_length(StrCpyVec* this);
static inline void StrCpyVec_clear(StrCpyVec* this);


// getters & setters
static inline char* StrCpyVec_at(StrCpyVec* this, size_t index);


struct StrCpyVec
{
   StrCpyList strCpyList;

   char** vecArray;
   size_t vecArrayLen;
};


void StrCpyVec_init(StrCpyVec* this)
{
   StrCpyList_init( (StrCpyList*)this);

   this->vecArrayLen = 4;
   this->vecArray = (char**)os_kmalloc(
      this->vecArrayLen * sizeof(char*) );
}

void StrCpyVec_uninit(StrCpyVec* this)
{
   kfree(this->vecArray);

   StrCpyList_uninit( (StrCpyList*)this);
}

void StrCpyVec_append(StrCpyVec* this, const char* valuePointer)
{
   PointerListElem* lastElem;
   char* lastElemValuePointer;

   StrCpyList_append( (StrCpyList*)this, valuePointer);

   // check if we have enough buffer space for new elem
   if(StrCpyList_length( (StrCpyList*)this) > this->vecArrayLen)
   { // double vector array size (create new, copy, exchange, delete old)
      char** newVecArray = (char**)os_kmalloc(this->vecArrayLen*sizeof(char*)*2);
      memcpy(newVecArray, this->vecArray, this->vecArrayLen*sizeof(char*) );
      kfree(this->vecArray);
      this->vecArrayLen = this->vecArrayLen * 2;
      this->vecArray = newVecArray;
   }

   // get last elem and add the valuePointer to the array
   lastElem = PointerList_getTail( (PointerList*)this);
   lastElemValuePointer = (char*)lastElem->valuePointer;
   (this->vecArray)[StrCpyList_length( (StrCpyList*)this)-1] = lastElemValuePointer;
}

size_t StrCpyVec_length(StrCpyVec* this)
{
   return StrCpyList_length( (StrCpyList*)this);
}

char* StrCpyVec_at(StrCpyVec* this, size_t index)
{
   BEEGFS_BUG_ON_DEBUG(index >= StrCpyVec_length(this), "Index out of bounds");

   return (this->vecArray)[index];
}

void StrCpyVec_clear(StrCpyVec* this)
{
   StrCpyList_clear( (StrCpyList*)this);
}

#endif /*STRCPYVEC_H_*/
