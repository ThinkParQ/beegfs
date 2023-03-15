#ifndef NICADDRESSLISTITER_H_
#define NICADDRESSLISTITER_H_

#include <common/Common.h>
#include "NicAddressList.h"

struct NicAddressListIter;
typedef struct NicAddressListIter NicAddressListIter;

static inline void NicAddressListIter_init(NicAddressListIter* this, NicAddressList* list);
static inline void NicAddressListIter_next(NicAddressListIter* this);
static inline NicAddress* NicAddressListIter_value(NicAddressListIter* this);
static inline bool NicAddressListIter_end(NicAddressListIter* this);
static inline NicAddressListIter NicAddressListIter_remove(NicAddressListIter* this);


struct NicAddressListIter
{
   PointerListIter pointerListIter;
};


void NicAddressListIter_init(NicAddressListIter* this, NicAddressList* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void NicAddressListIter_next(NicAddressListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

NicAddress* NicAddressListIter_value(NicAddressListIter* this)
{
   return (NicAddress*)PointerListIter_value( (PointerListIter*)this);
}

bool NicAddressListIter_end(NicAddressListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}

/**
 * note: the current iterator becomes invalid after the call (use the returned iterator)
 * @return the new iterator that points to the element just behind the erased one
 */
NicAddressListIter NicAddressListIter_remove(NicAddressListIter* this)
{
   NicAddressListIter newIter = *this;

   NicAddressListIter_next(&newIter); // the new iter that will be returned

   PointerListIter_remove( (PointerListIter*)this);

   return newIter;
}


#endif /*NICADDRESSLISTITER_H_*/
