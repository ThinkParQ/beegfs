#ifndef NICADDRESSLIST_H_
#define NICADDRESSLIST_H_

#include <common/toolkit/list/PointerList.h>
#include <common/Common.h>
#include "NicAddress.h"

struct NicAddressList;
typedef struct NicAddressList NicAddressList;

static inline void NicAddressList_init(NicAddressList* this);
static inline void NicAddressList_uninit(NicAddressList* this);
static inline void NicAddressList_append(NicAddressList* this, NicAddress* nicAddress);
static inline size_t NicAddressList_length(NicAddressList* this);

struct NicAddressList
{
   struct PointerList pointerList;
};


void NicAddressList_init(NicAddressList* this)
{
   PointerList_init( (PointerList*)this);
}

void NicAddressList_uninit(NicAddressList* this)
{
   PointerList_uninit( (PointerList*)this);
}

void NicAddressList_append(NicAddressList* this, NicAddress* nicAddress)
{
   PointerList_append( (PointerList*)this, nicAddress);
}

static inline size_t NicAddressList_length(NicAddressList* this)
{
   return PointerList_length( (PointerList*)this);
}

#endif /*NICADDRESSLIST_H_*/
