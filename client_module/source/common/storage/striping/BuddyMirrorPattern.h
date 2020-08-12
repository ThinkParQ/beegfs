#ifndef BUDDYMIRRORPATTERN_H_
#define BUDDYMIRRORPATTERN_H_

#include <common/toolkit/Serialization.h>
#include "StripePattern.h"

struct BuddyMirrorPattern;
typedef struct BuddyMirrorPattern BuddyMirrorPattern;


static inline void BuddyMirrorPattern_init(BuddyMirrorPattern* this,
   unsigned chunkSize, UInt16Vec* mirrorBuddyGroupIDs, unsigned defaultNumTargets);
static inline void BuddyMirrorPattern_initFromChunkSize(BuddyMirrorPattern* this,
   unsigned chunkSize);
static inline BuddyMirrorPattern* BuddyMirrorPattern_construct(
   unsigned chunkSize, UInt16Vec* mirrorBuddyGroupIDs, unsigned defaultNumTargets);
static inline BuddyMirrorPattern* BuddyMirrorPattern_constructFromChunkSize(unsigned chunkSize);
static inline void BuddyMirrorPattern_uninit(StripePattern* this);

static inline void __BuddyMirrorPattern_assignVirtualFunctions(BuddyMirrorPattern* this);

// virtual functions
extern bool BuddyMirrorPattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx);

extern size_t BuddyMirrorPattern_getStripeTargetIndex(StripePattern* this, int64_t pos);
extern uint16_t BuddyMirrorPattern_getStripeTargetID(StripePattern* this, int64_t pos);
extern void BuddyMirrorPattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs);
extern UInt16Vec* BuddyMirrorPattern_getStripeTargetIDs(StripePattern* this);
extern unsigned BuddyMirrorPattern_getMinNumTargets(StripePattern* this);
extern unsigned BuddyMirrorPattern_getDefaultNumTargets(StripePattern* this);



struct BuddyMirrorPattern
{
   StripePattern stripePattern;

   UInt16Vec mirrorBuddyGroupIDs;
   unsigned defaultNumTargets;
};


/**
 * @param mirrorBuddyGroupIDs will be copied
 * @param defaultNumTargets default number of targets (0 for app-level default)
 */
void BuddyMirrorPattern_init(BuddyMirrorPattern* this,
   unsigned chunkSize, UInt16Vec* mirrorBuddyGroupIDs, unsigned defaultNumTargets)
{
   StripePattern_initFromPatternType( (StripePattern*)this, STRIPEPATTERN_BuddyMirror, chunkSize);

   // assign virtual functions
   __BuddyMirrorPattern_assignVirtualFunctions(this);

   // init attribs
   UInt16Vec_init(&this->mirrorBuddyGroupIDs);

   ListTk_copyUInt16ListToVec( (UInt16List*)mirrorBuddyGroupIDs, &this->mirrorBuddyGroupIDs);

   this->defaultNumTargets = defaultNumTargets ? defaultNumTargets : 4;
}

/**
 * Note: for deserialization only
 */
void BuddyMirrorPattern_initFromChunkSize(BuddyMirrorPattern* this, unsigned chunkSize)
{
   StripePattern_initFromPatternType( (StripePattern*)this, STRIPEPATTERN_BuddyMirror, chunkSize);

   // assign virtual functions
   __BuddyMirrorPattern_assignVirtualFunctions(this);

   // init attribs
   UInt16Vec_init(&this->mirrorBuddyGroupIDs);
}

/**
 * @param mirrorBuddyGroupIDs will be copied
 * @param defaultNumTargets default number of targets (0 for app-level default)
 */
BuddyMirrorPattern* BuddyMirrorPattern_construct(
   unsigned chunkSize, UInt16Vec* mirrorBuddyGroupIDs, unsigned defaultNumTargets)
{
   struct BuddyMirrorPattern* this = os_kmalloc(sizeof(*this) );

   BuddyMirrorPattern_init(this, chunkSize, mirrorBuddyGroupIDs, defaultNumTargets);

   return this;
}

/**
 * Note: for deserialization only
 */
BuddyMirrorPattern* BuddyMirrorPattern_constructFromChunkSize(unsigned chunkSize)
{
   struct BuddyMirrorPattern* this = os_kmalloc(sizeof(*this) );

   BuddyMirrorPattern_initFromChunkSize(this, chunkSize);

   return this;
}

void BuddyMirrorPattern_uninit(StripePattern* this)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   UInt16Vec_uninit(&thisCast->mirrorBuddyGroupIDs);
}

void __BuddyMirrorPattern_assignVirtualFunctions(BuddyMirrorPattern* this)
{
   ( (StripePattern*)this)->uninit = BuddyMirrorPattern_uninit;

   ( (StripePattern*)this)->deserializePattern = BuddyMirrorPattern_deserializePattern;

   ( (StripePattern*)this)->getStripeTargetIndex = BuddyMirrorPattern_getStripeTargetIndex;
   ( (StripePattern*)this)->getStripeTargetID = BuddyMirrorPattern_getStripeTargetID;
   ( (StripePattern*)this)->getStripeTargetIDsCopy = BuddyMirrorPattern_getStripeTargetIDsCopy;
   ( (StripePattern*)this)->getStripeTargetIDs = BuddyMirrorPattern_getStripeTargetIDs;
   ( (StripePattern*)this)->getMinNumTargets = BuddyMirrorPattern_getMinNumTargets;
   ( (StripePattern*)this)->getDefaultNumTargets = BuddyMirrorPattern_getDefaultNumTargets;
}

#endif /*BUDDYMIRRORPATTERN_H_*/
