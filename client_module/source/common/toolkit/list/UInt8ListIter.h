#ifndef UINT8LISTITER_H_
#define UINT8LISTITER_H_

#include <common/toolkit/list/PointerListIter.h>
#include "UInt8List.h"

struct UInt8ListIter;
typedef struct UInt8ListIter UInt8ListIter;

static inline void UInt8ListIter_init(UInt8ListIter* this, UInt8List* list);
static inline void UInt8ListIter_next(UInt8ListIter* this);
static inline uint8_t UInt8ListIter_value(UInt8ListIter* this);
static inline bool UInt8ListIter_end(UInt8ListIter* this);


struct UInt8ListIter
{
   struct PointerListIter pointerListIter;
};


void UInt8ListIter_init(UInt8ListIter* this, UInt8List* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void UInt8ListIter_next(UInt8ListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

uint8_t UInt8ListIter_value(UInt8ListIter* this)
{
   return (uint8_t)(size_t)PointerListIter_value( (PointerListIter*)this);
}

bool UInt8ListIter_end(UInt8ListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}


#endif /* UINT8LISTITER_H_ */
