#include <common/toolkit/MathTk.h>
#include "BuddyMirrorPattern.h"

bool BuddyMirrorPattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   RawList mirrorBuddyGroupIDsVec;

   // defaultNumTargets
   if(!Serialization_deserializeUInt(ctx, &thisCast->defaultNumTargets) )
      return false;

   // mirrorBuddyGroupIDs
   if(!Serialization_deserializeUInt16VecPreprocess(ctx, &mirrorBuddyGroupIDsVec) )
      return false;

   if(!Serialization_deserializeUInt16Vec(&mirrorBuddyGroupIDsVec, &thisCast->mirrorBuddyGroupIDs) )
      return false;

   // check mirrorBuddyGroupIDs
   if(!UInt16Vec_length(&thisCast->mirrorBuddyGroupIDs) )
      return false;

   return true;
}

size_t BuddyMirrorPattern_getStripeTargetIndex(StripePattern* this, int64_t pos)
{
   struct BuddyMirrorPattern* p = container_of(this, struct BuddyMirrorPattern, stripePattern);

   return (pos / this->chunkSize) % UInt16Vec_length(&p->mirrorBuddyGroupIDs);
}

uint16_t BuddyMirrorPattern_getStripeTargetID(StripePattern* this, int64_t pos)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   size_t targetIndex = BuddyMirrorPattern_getStripeTargetIndex(this, pos);

   return UInt16Vec_at(&thisCast->mirrorBuddyGroupIDs, targetIndex);
}

void BuddyMirrorPattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   ListTk_copyUInt16ListToVec( (UInt16List*)&thisCast->mirrorBuddyGroupIDs, outTargetIDs);
}

UInt16Vec* BuddyMirrorPattern_getStripeTargetIDs(StripePattern* this)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   return &thisCast->mirrorBuddyGroupIDs;
}

unsigned BuddyMirrorPattern_getMinNumTargets(StripePattern* this)
{
   return 1;
}

unsigned BuddyMirrorPattern_getDefaultNumTargets(StripePattern* this)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   return thisCast->defaultNumTargets;
}


