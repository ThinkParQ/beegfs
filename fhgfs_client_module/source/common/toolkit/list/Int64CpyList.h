#ifndef INT64CPYLIST_H_
#define INT64CPYLIST_H_

#include <common/toolkit/list/PointerList.h>

/**
 * We need to copy int64 values (instead of just assigning them to just to the internal pointer)
 * because we might be running on 32bit archs.
 */

struct Int64CpyList;
typedef struct Int64CpyList Int64CpyList;

static inline void Int64CpyList_init(Int64CpyList* this);
static inline void Int64CpyList_uninit(Int64CpyList* this);
static inline void Int64CpyList_append(Int64CpyList* this, int64_t value);
static inline size_t Int64CpyList_length(Int64CpyList* this);
static inline void Int64CpyList_clear(Int64CpyList* this);

struct Int64CpyList
{
   struct PointerList pointerList;
};


void Int64CpyList_init(Int64CpyList* this)
{
   PointerList_init( (PointerList*)this);
}

void Int64CpyList_uninit(Int64CpyList* this)
{
   struct PointerListElem* elem = ( (PointerList*)this)->head;
   while(elem)
   {
      struct PointerListElem* next = elem->next;
      kfree(elem->valuePointer);
      elem = next;
   }


   PointerList_uninit( (PointerList*)this);
}

void Int64CpyList_append(Int64CpyList* this, int64_t value)
{
   int64_t* valueCopyPointer = (int64_t*)os_kmalloc(sizeof(int64_t) );

   *valueCopyPointer = value;

   PointerList_append( (PointerList*)this, valueCopyPointer);
}

static inline size_t Int64CpyList_length(Int64CpyList* this)
{
   return PointerList_length( (PointerList*)this);
}

void Int64CpyList_clear(Int64CpyList* this)
{
   struct PointerListElem* elem = ( (PointerList*)this)->head;
   while(elem)
   {
      struct PointerListElem* next = elem->next;
      kfree(elem->valuePointer);
      elem = next;
   }

   PointerList_clear( (PointerList*)this);
}


#endif /*INT64CPYLIST_H_*/
