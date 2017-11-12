#ifndef UINT8LIST_H_
#define UINT8LIST_H_

#include <common/toolkit/list/PointerList.h>

/**
 * Derived from PointerList. Internally, we cast uint8_t values directly to pointers here (instead
 * of allocating them and assigning the pointer to the allocated mem here).
 */

struct UInt8List;
typedef struct UInt8List UInt8List;

static inline void UInt8List_init(UInt8List* this);
static inline void UInt8List_uninit(UInt8List* this);
static inline void UInt8List_append(UInt8List* this, uint8_t value);
static inline size_t UInt8List_length(UInt8List* this);
static inline void UInt8List_clear(UInt8List* this);

struct UInt8List
{
   struct PointerList pointerList;
};


void UInt8List_init(UInt8List* this)
{
   PointerList_init( (PointerList*)this);
}

void UInt8List_uninit(UInt8List* this)
{
   PointerList_uninit( (PointerList*)this);
}

void UInt8List_append(UInt8List* this, uint8_t value)
{
   /* cast value directly to pointer type here to store value directly in the pointer variable
      without allocating extra mem */
   PointerList_append( (PointerList*)this, (void*)(size_t)value);
}

static inline size_t UInt8List_length(UInt8List* this)
{
   return PointerList_length( (PointerList*)this);
}

void UInt8List_clear(UInt8List* this)
{
   PointerList_clear( (PointerList*)this);
}

#endif /* UINT8LIST_H_ */
