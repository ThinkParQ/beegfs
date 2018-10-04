#ifndef STRCPYLISTITER_H_
#define STRCPYLISTITER_H_

#include "StrCpyList.h"
#include "StringListIter.h"

struct StrCpyListIter;
typedef struct StrCpyListIter StrCpyListIter;

static inline void StrCpyListIter_init(StrCpyListIter* this, StrCpyList* list);
static inline void StrCpyListIter_next(StrCpyListIter* this);
static inline char* StrCpyListIter_value(StrCpyListIter* this);
static inline bool StrCpyListIter_end(StrCpyListIter* this);


struct StrCpyListIter
{
   struct StringListIter stringListIter;
};


void StrCpyListIter_init(StrCpyListIter* this, StrCpyList* list)
{
   StringListIter_init( (StringListIter*)this, (StringList*)list);
}

void StrCpyListIter_next(StrCpyListIter* this)
{
   StringListIter_next( (StringListIter*)this);
}

char* StrCpyListIter_value(StrCpyListIter* this)
{
   return (char*)StringListIter_value( (StringListIter*)this);
}

bool StrCpyListIter_end(StrCpyListIter* this)
{
   return StringListIter_end( (StringListIter*)this);
}


#endif /*STRCPYLISTITER_H_*/
