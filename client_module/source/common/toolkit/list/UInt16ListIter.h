#ifndef UINT16LISTITER_H_
#define UINT16LISTITER_H_

#include <common/toolkit/list/PointerListIter.h>
#include "UInt16List.h"

struct UInt16ListIter;
typedef struct UInt16ListIter UInt16ListIter;

static inline void UInt16ListIter_init(UInt16ListIter* this, UInt16List* list);
static inline void UInt16ListIter_next(UInt16ListIter* this);
static inline uint16_t UInt16ListIter_value(UInt16ListIter* this);
static inline bool UInt16ListIter_end(UInt16ListIter* this);


struct UInt16ListIter
{
   struct PointerListIter pointerListIter;
};


void UInt16ListIter_init(UInt16ListIter* this, UInt16List* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void UInt16ListIter_next(UInt16ListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

uint16_t UInt16ListIter_value(UInt16ListIter* this)
{
   return (uint16_t)(size_t)PointerListIter_value( (PointerListIter*)this);
}

bool UInt16ListIter_end(UInt16ListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}


#endif /* UINT16LISTITER_H_ */
