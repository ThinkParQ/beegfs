#ifndef NICADDRESSSTATSLIST_H_
#define NICADDRESSSTATSLIST_H_

#include <common/Common.h>
#include <common/toolkit/list/PointerList.h>
#include "NicAddressStats.h"

struct NicAddressStatsList;
typedef struct NicAddressStatsList NicAddressStatsList;

static inline void NicAddressStatsList_init(NicAddressStatsList* this);
static inline void NicAddressStatsList_uninit(NicAddressStatsList* this);
static inline void NicAddressStatsList_append(NicAddressStatsList* this, NicAddressStats* stats);
static inline size_t NicAddressStatsList_length(NicAddressStatsList* this);

struct NicAddressStatsList
{
   PointerList pointerList;
};

void NicAddressStatsList_init(NicAddressStatsList* this)
{
   PointerList_init( (PointerList*)this);
}

void NicAddressStatsList_uninit(NicAddressStatsList* this)
{
   PointerList_uninit( (PointerList*)this);
}

void NicAddressStatsList_append(NicAddressStatsList* this, struct NicAddressStats* stats)
{
   PointerList_append( (PointerList*)this, stats);
}

static inline size_t NicAddressStatsList_length(NicAddressStatsList* this)
{
   return PointerList_length( (PointerList*)this);
}


#endif /*NICADDRESSSTATSLIST_H_*/
