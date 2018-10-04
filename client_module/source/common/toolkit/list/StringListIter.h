#ifndef STRINGLISTITER_H_
#define STRINGLISTITER_H_

#include "StringList.h"
#include "PointerListIter.h"

struct StringListIter;
typedef struct StringListIter StringListIter;

static inline void StringListIter_init(StringListIter* this, StringList* list);
static inline void StringListIter_next(StringListIter* this);
static inline char* StringListIter_value(StringListIter* this);
static inline bool StringListIter_end(StringListIter* this);


struct StringListIter
{
   PointerListIter pointerListIter;
};


void StringListIter_init(StringListIter* this, StringList* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void StringListIter_next(StringListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

char* StringListIter_value(StringListIter* this)
{
   return (char*)PointerListIter_value( (PointerListIter*)this);
}

bool StringListIter_end(StringListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}


#endif /*STRINGLISTITER_H_*/
