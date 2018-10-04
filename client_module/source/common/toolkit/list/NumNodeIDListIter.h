#ifndef NUMNODEIDLISTITER_H_
#define NUMNODEIDLISTITER_H_

#include <common/toolkit/list/PointerListIter.h>
#include "NumNodeIDList.h"

struct NumNodeIDListIter;
typedef struct NumNodeIDListIter NumNodeIDListIter;

static inline void NumNodeIDListIter_init(NumNodeIDListIter* this, NumNodeIDList* list);
static inline void NumNodeIDListIter_next(NumNodeIDListIter* this);
static inline NumNodeID NumNodeIDListIter_value(NumNodeIDListIter* this);
static inline bool NumNodeIDListIter_end(NumNodeIDListIter* this);


struct NumNodeIDListIter
{
   struct PointerListIter pointerListIter;
};


void NumNodeIDListIter_init(NumNodeIDListIter* this, NumNodeIDList* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void NumNodeIDListIter_next(NumNodeIDListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

NumNodeID NumNodeIDListIter_value(NumNodeIDListIter* this)
{
   return (NumNodeID){(size_t)PointerListIter_value( (PointerListIter*)this)};
}

bool NumNodeIDListIter_end(NumNodeIDListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}


#endif /* NUMNODEIDLISTITER_H_ */
