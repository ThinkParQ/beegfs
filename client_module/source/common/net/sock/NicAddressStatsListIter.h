#ifndef NICADDRESSSTATSLISTITER_H_
#define NICADDRESSSTATSLISTITER_H_

#include <common/toolkit/list/PointerListIter.h>
#include "NicAddressStatsList.h"

struct NicAddressStatsListIter;
typedef struct NicAddressStatsListIter NicAddressStatsListIter;

static inline void NicAddressStatsListIter_init(NicAddressStatsListIter* this, NicAddressStatsList* list);
static inline void NicAddressStatsListIter_next(NicAddressStatsListIter* this);
static inline NicAddressStats* NicAddressStatsListIter_value(NicAddressStatsListIter* this);
static inline bool NicAddressStatsListIter_end(NicAddressStatsListIter* this);
static inline NicAddressStatsListIter NicAddressStatsListIter_remove(NicAddressStatsListIter* this);

struct NicAddressStatsListIter
{
   PointerListIter pointerListIter;
};

void NicAddressStatsListIter_init(NicAddressStatsListIter* this, NicAddressStatsList* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void NicAddressStatsListIter_next(NicAddressStatsListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

NicAddressStats* NicAddressStatsListIter_value(NicAddressStatsListIter* this)
{
   return (struct NicAddressStats*)PointerListIter_value( (PointerListIter*)this);
}

bool NicAddressStatsListIter_end(NicAddressStatsListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}

/**
 * note: the current iterator becomes invalid after the call (use the returned iterator)
 * @return the new iterator that points to the element just behind the erased one
 */
NicAddressStatsListIter NicAddressStatsListIter_remove(NicAddressStatsListIter* this)
{
   NicAddressStatsListIter newIter = *this;

   NicAddressStatsListIter_next(&newIter); // the new iter that will be returned

   PointerListIter_remove( (PointerListIter*)this);

   return newIter;
}

#endif /*NICADDRESSSTATSLISTITER_H_*/
