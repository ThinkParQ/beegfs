#ifndef INT64CPYLISTITER_H_
#define INT64CPYLISTITER_H_

#include "Int64CpyList.h"
#include "PointerListIter.h"

struct Int64CpyListIter;
typedef struct Int64CpyListIter Int64CpyListIter;

static inline void Int64CpyListIter_init(Int64CpyListIter* this, Int64CpyList* list);
static inline void Int64CpyListIter_next(Int64CpyListIter* this);
static inline int64_t Int64CpyListIter_value(Int64CpyListIter* this);
static inline bool Int64CpyListIter_end(Int64CpyListIter* this);


struct Int64CpyListIter
{
   struct PointerListIter pointerListIter;
};


void Int64CpyListIter_init(Int64CpyListIter* this, Int64CpyList* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void Int64CpyListIter_next(Int64CpyListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

int64_t Int64CpyListIter_value(Int64CpyListIter* this)
{
   return *(int64_t*)PointerListIter_value( (PointerListIter*)this);
}

bool Int64CpyListIter_end(Int64CpyListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}


#endif /*INT64CPYLISTITER_H_*/
