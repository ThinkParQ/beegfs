#ifndef RAID10PATTERN_H_
#define RAID10PATTERN_H_

#include <common/toolkit/Serialization.h>
#include "StripePattern.h"

struct Raid10Pattern;
typedef struct Raid10Pattern Raid10Pattern;


static inline void Raid10Pattern_initFromChunkSize(Raid10Pattern* this, unsigned chunkSize);
static inline Raid10Pattern* Raid10Pattern_constructFromChunkSize(unsigned chunkSize);
static inline void Raid10Pattern_uninit(StripePattern* this);

static inline void __Raid10Pattern_assignVirtualFunctions(Raid10Pattern* this);

// virtual functions
extern bool Raid10Pattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx);

extern size_t Raid10Pattern_getStripeTargetIndex(StripePattern* this, int64_t pos);
extern uint16_t Raid10Pattern_getStripeTargetID(StripePattern* this, int64_t pos);
extern void Raid10Pattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs);
extern UInt16Vec* Raid10Pattern_getStripeTargetIDs(StripePattern* this);
extern UInt16Vec* Raid10Pattern_getMirrorTargetIDs(StripePattern* this);
extern unsigned Raid10Pattern_getMinNumTargets(StripePattern* this);
extern unsigned Raid10Pattern_getDefaultNumTargets(StripePattern* this);


/**
 * Note: We don't have the general _construct() and _init() methods implemented, because we just
 * don't need them at the moment in the client. (We only have the special _constructFrom...() and
 * _initFrom...() for deserialization.)
 */
struct Raid10Pattern
{
   StripePattern stripePattern;

   UInt16Vec stripeTargetIDs;
   UInt16Vec mirrorTargetIDs;
   unsigned stripeSetSize; // = numStripeTargets * chunkSize
   unsigned defaultNumTargets;
};


/**
 * Note: for deserialization only
 */
void Raid10Pattern_initFromChunkSize(Raid10Pattern* this, unsigned chunkSize)
{
   StripePattern_initFromPatternType( (StripePattern*)this, STRIPEPATTERN_Raid10, chunkSize);

   // assign virtual functions
   __Raid10Pattern_assignVirtualFunctions(this);

   // init attribs
   UInt16Vec_init(&this->stripeTargetIDs);
   UInt16Vec_init(&this->mirrorTargetIDs);
}

/**
 * Note: for deserialization only
 */
Raid10Pattern* Raid10Pattern_constructFromChunkSize(unsigned chunkSize)
{
   struct Raid10Pattern* this = os_kmalloc(sizeof(*this) );

   Raid10Pattern_initFromChunkSize(this, chunkSize);

   return this;
}

void Raid10Pattern_uninit(StripePattern* this)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   UInt16Vec_uninit(&thisCast->stripeTargetIDs);
   UInt16Vec_uninit(&thisCast->mirrorTargetIDs);
}

void __Raid10Pattern_assignVirtualFunctions(Raid10Pattern* this)
{
   ( (StripePattern*)this)->uninit = Raid10Pattern_uninit;

   ( (StripePattern*)this)->deserializePattern = Raid10Pattern_deserializePattern;

   ( (StripePattern*)this)->getStripeTargetIndex = Raid10Pattern_getStripeTargetIndex;
   ( (StripePattern*)this)->getStripeTargetID = Raid10Pattern_getStripeTargetID;
   ( (StripePattern*)this)->getStripeTargetIDsCopy = Raid10Pattern_getStripeTargetIDsCopy;
   ( (StripePattern*)this)->getStripeTargetIDs = Raid10Pattern_getStripeTargetIDs;
   ( (StripePattern*)this)->getMirrorTargetIDs = Raid10Pattern_getMirrorTargetIDs;
   ( (StripePattern*)this)->getMinNumTargets = Raid10Pattern_getMinNumTargets;
   ( (StripePattern*)this)->getDefaultNumTargets = Raid10Pattern_getDefaultNumTargets;
}


#endif /* RAID10PATTERN_H_ */
