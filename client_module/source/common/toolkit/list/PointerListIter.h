#ifndef POINTERLISTITER_H_
#define POINTERLISTITER_H_

#include "PointerList.h"

struct PointerListIter;
typedef struct PointerListIter PointerListIter;

static inline void PointerListIter_init(PointerListIter* this, PointerList* list);
static inline void PointerListIter_next(PointerListIter* this);
static inline void* PointerListIter_value(PointerListIter* this);
static inline bool PointerListIter_end(PointerListIter* this);
static inline PointerListIter PointerListIter_remove(PointerListIter* this);

struct PointerListIter
{
   PointerList* list;
   PointerListElem* elem;
};

void PointerListIter_init(PointerListIter* this, PointerList* list)
{
   this->list = list;
   this->elem = list->head;
}

void PointerListIter_next(PointerListIter* this)
{
   // note: must not return the value because the current elem could be the end of the list

   this->elem = this->elem->next;
}

void* PointerListIter_value(PointerListIter* this)
{
   return this->elem->valuePointer;
}

bool PointerListIter_end(PointerListIter* this)
{
   return (this->elem == NULL);
}

/**
 * note: the current iterator becomes invalid after the call (use the returned iterator)
 * @return the new iterator that points to the element just behind the erased one
 */
PointerListIter PointerListIter_remove(PointerListIter* this)
{
   PointerListIter newIter = *this;

   PointerListElem* elem = this->elem;

   PointerListIter_next(&newIter);

   PointerList_removeElem(this->list, elem);

   return newIter;
}


#endif /*POINTERLISTITER_H_*/
