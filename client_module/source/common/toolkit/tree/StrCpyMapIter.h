#ifndef STRCPYMAPITER_H_
#define STRCPYMAPITER_H_

#include "StrCpyMap.h"

struct StrCpyMapIter;
typedef struct StrCpyMapIter StrCpyMapIter;

static inline void StrCpyMapIter_init(StrCpyMapIter* this, StrCpyMap* map, RBTreeElem* treeElem);
static inline char* StrCpyMapIter_next(StrCpyMapIter* this);
static inline char* StrCpyMapIter_key(StrCpyMapIter* this);
static inline char* StrCpyMapIter_value(StrCpyMapIter* this);
static inline bool StrCpyMapIter_end(StrCpyMapIter* this);

struct StrCpyMapIter
{
   RBTreeIter rbTreeIter;
};

void StrCpyMapIter_init(StrCpyMapIter* this, StrCpyMap* map, RBTreeElem* treeElem)
{
   PointerRBTreeIter_init( (RBTreeIter*)this, (RBTree*)map, (RBTreeElem*)treeElem);
}

char* StrCpyMapIter_next(StrCpyMapIter* this)
{
   return (char*)PointerRBTreeIter_next( (RBTreeIter*)this);
}

char* StrCpyMapIter_key(StrCpyMapIter* this)
{
   return (char*)PointerRBTreeIter_key( (RBTreeIter*)this);
}

char* StrCpyMapIter_value(StrCpyMapIter* this)
{
   return (char*)PointerRBTreeIter_value( (RBTreeIter*)this);
}

bool StrCpyMapIter_end(StrCpyMapIter* this)
{
   return PointerRBTreeIter_end( (RBTreeIter*)this);
}

#endif /*STRCPYMAPITER_H_*/
