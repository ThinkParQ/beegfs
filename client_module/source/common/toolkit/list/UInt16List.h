#ifndef UINT16LIST_H_
#define UINT16LIST_H_

#include <common/toolkit/list/PointerList.h>

/**
 * Derived from PointerList. Internally, we cast uint16_t values directly to pointers here (instead
 * of allocating them and assigning the pointer to the allocated mem here).
 */

struct UInt16List;
typedef struct UInt16List UInt16List;

static inline void UInt16List_init(UInt16List* this);
static inline void UInt16List_uninit(UInt16List* this);
static inline void UInt16List_append(UInt16List* this, uint16_t value);
static inline size_t UInt16List_length(UInt16List* this);
static inline void UInt16List_clear(UInt16List* this);

struct UInt16List
{
   struct PointerList pointerList;
};


void UInt16List_init(UInt16List* this)
{
   PointerList_init( (PointerList*)this);
}

void UInt16List_uninit(UInt16List* this)
{
   PointerList_uninit( (PointerList*)this);
}

void UInt16List_append(UInt16List* this, uint16_t value)
{
   /* cast value directly to pointer type here to store value directly in the pointer variable
      without allocating extra mem */
   PointerList_append( (PointerList*)this, (void*)(size_t)value);
}

static inline size_t UInt16List_length(UInt16List* this)
{
   return PointerList_length( (PointerList*)this);
}

void UInt16List_clear(UInt16List* this)
{
   PointerList_clear( (PointerList*)this);
}

#endif /* UINT16LIST_H_ */
