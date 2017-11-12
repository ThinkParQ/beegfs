#ifndef NUMNODEIDLIST_H_
#define NUMNODEIDLIST_H_

#include <common/nodes/NumNodeID.h>
#include <common/toolkit/list/PointerList.h>

/**
 * Derived from PointerList. Internally, we cast NumNodeID values directly to pointers here (instead
 * of allocating them and assigning the pointer to the allocated mem here).
 */

struct NumNodeIDList;
typedef struct NumNodeIDList NumNodeIDList;

static inline void NumNodeIDList_init(NumNodeIDList* this);
static inline void NumNodeIDList_uninit(NumNodeIDList* this);
static inline void NumNodeIDList_append(NumNodeIDList* this, NumNodeID value);
static inline size_t NumNodeIDList_length(NumNodeIDList* this);
static inline void NumNodeIDList_clear(NumNodeIDList* this);

struct NumNodeIDList
{
   struct PointerList pointerList;
};


void NumNodeIDList_init(NumNodeIDList* this)
{
   PointerList_init( (PointerList*)this);
}

void NumNodeIDList_uninit(NumNodeIDList* this)
{
   PointerList_uninit( (PointerList*)this);
}

void NumNodeIDList_append(NumNodeIDList* this, NumNodeID value)
{
   /* cast value directly to pointer type here to store value directly in the pointer variable
      without allocating extra mem */
   PointerList_append( (PointerList*)this, (void*)(size_t)value.value);
}

static inline size_t NumNodeIDList_length(NumNodeIDList* this)
{
   return PointerList_length( (PointerList*)this);
}

void NumNodeIDList_clear(NumNodeIDList* this)
{
   PointerList_clear( (PointerList*)this);
}

#endif /* NUMNODEIDLIST_H_ */
