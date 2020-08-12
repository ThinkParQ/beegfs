#ifndef RAID0PATTERN_H_
#define RAID0PATTERN_H_

#include <common/toolkit/Serialization.h>
#include "StripePattern.h"

struct Raid0Pattern;
typedef struct Raid0Pattern Raid0Pattern;


static inline void Raid0Pattern_init(Raid0Pattern* this,
   unsigned chunkSize, UInt16Vec* stripeTargetIDs, unsigned defaultNumTargets);
static inline void Raid0Pattern_initFromChunkSize(Raid0Pattern* this, unsigned chunkSize);
static inline Raid0Pattern* Raid0Pattern_construct(
   unsigned chunkSize, UInt16Vec* stripeTargetIDs, unsigned defaultNumTargets);
static inline Raid0Pattern* Raid0Pattern_constructFromChunkSize(unsigned chunkSize);
static inline void Raid0Pattern_uninit(StripePattern* this);

static inline void __Raid0Pattern_assignVirtualFunctions(Raid0Pattern* this);

// virtual functions
extern bool Raid0Pattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx);

extern size_t Raid0Pattern_getStripeTargetIndex(StripePattern* this, int64_t pos);
extern uint16_t Raid0Pattern_getStripeTargetID(StripePattern* this, int64_t pos);
extern void Raid0Pattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs);
extern UInt16Vec* Raid0Pattern_getStripeTargetIDs(StripePattern* this);
extern unsigned Raid0Pattern_getMinNumTargets(StripePattern* this);
extern unsigned Raid0Pattern_getDefaultNumTargets(StripePattern* this);



struct Raid0Pattern
{
   StripePattern stripePattern;

   UInt16Vec stripeTargetIDs;
   unsigned defaultNumTargets;
};


/**
 * @param stripeTargetIDs will be copied
 * @param defaultNumTargets default number of targets (0 for app-level default)
 */
void Raid0Pattern_init(Raid0Pattern* this,
   unsigned chunkSize, UInt16Vec* stripeTargetIDs, unsigned defaultNumTargets)
{
   StripePattern_initFromPatternType( (StripePattern*)this, STRIPEPATTERN_Raid0, chunkSize);

   // assign virtual functions
   __Raid0Pattern_assignVirtualFunctions(this);

   // init attribs
   UInt16Vec_init(&this->stripeTargetIDs);

   ListTk_copyUInt16ListToVec( (UInt16List*)stripeTargetIDs, &this->stripeTargetIDs);

   this->defaultNumTargets = defaultNumTargets ? defaultNumTargets : 4;

}

/**
 * Note: for deserialization only
 */
void Raid0Pattern_initFromChunkSize(Raid0Pattern* this, unsigned chunkSize)
{
   StripePattern_initFromPatternType( (StripePattern*)this, STRIPEPATTERN_Raid0, chunkSize);

   // assign virtual functions
   __Raid0Pattern_assignVirtualFunctions(this);

   // init attribs
   UInt16Vec_init(&this->stripeTargetIDs);
}

/**
 * @param stripeTargetIDs will be copied
 * @param defaultNumTargets default number of targets (0 for app-level default)
 */
Raid0Pattern* Raid0Pattern_construct(
   unsigned chunkSize, UInt16Vec* stripeTargetIDs, unsigned defaultNumTargets)
{
   struct Raid0Pattern* this = os_kmalloc(sizeof(*this) );

   Raid0Pattern_init(this, chunkSize, stripeTargetIDs, defaultNumTargets);

   return this;
}

/**
 * Note: for deserialization only
 */
Raid0Pattern* Raid0Pattern_constructFromChunkSize(unsigned chunkSize)
{
   struct Raid0Pattern* this = os_kmalloc(sizeof(*this) );

   Raid0Pattern_initFromChunkSize(this, chunkSize);

   return this;
}

void Raid0Pattern_uninit(StripePattern* this)
{
   Raid0Pattern* thisCast = (Raid0Pattern*)this;

   UInt16Vec_uninit(&thisCast->stripeTargetIDs);
}

void __Raid0Pattern_assignVirtualFunctions(Raid0Pattern* this)
{
   ( (StripePattern*)this)->uninit = Raid0Pattern_uninit;

   ( (StripePattern*)this)->deserializePattern = Raid0Pattern_deserializePattern;

   ( (StripePattern*)this)->getStripeTargetIndex = Raid0Pattern_getStripeTargetIndex;
   ( (StripePattern*)this)->getStripeTargetID = Raid0Pattern_getStripeTargetID;
   ( (StripePattern*)this)->getStripeTargetIDsCopy = Raid0Pattern_getStripeTargetIDsCopy;
   ( (StripePattern*)this)->getStripeTargetIDs = Raid0Pattern_getStripeTargetIDs;
   ( (StripePattern*)this)->getMinNumTargets = Raid0Pattern_getMinNumTargets;
   ( (StripePattern*)this)->getDefaultNumTargets = Raid0Pattern_getDefaultNumTargets;
}

#endif /*RAID0PATTERN_H_*/
