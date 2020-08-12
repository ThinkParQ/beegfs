#include <common/toolkit/MathTk.h>
#include "Raid0Pattern.h"

bool Raid0Pattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx)
{
   Raid0Pattern* thisCast = (Raid0Pattern*)this;

   RawList targetIDsList;

   // defaultNumTargets
   if(!Serialization_deserializeUInt(ctx, &thisCast->defaultNumTargets) )
      return false;

   // targetIDs
   if(!Serialization_deserializeUInt16VecPreprocess(ctx, &targetIDsList) )
      return false;

   if(!Serialization_deserializeUInt16Vec(&targetIDsList, &thisCast->stripeTargetIDs) )
      return false;

   // check targetIDs
   if(!UInt16Vec_length(&thisCast->stripeTargetIDs) )
      return false;

   return true;
}

size_t Raid0Pattern_getStripeTargetIndex(StripePattern* this, int64_t pos)
{
   struct Raid0Pattern* p = container_of(this, struct Raid0Pattern, stripePattern);

   return (pos / this->chunkSize) % UInt16Vec_length(&p->stripeTargetIDs);
}

uint16_t Raid0Pattern_getStripeTargetID(StripePattern* this, int64_t pos)
{
   Raid0Pattern* thisCast = (Raid0Pattern*)this;

   size_t targetIndex = Raid0Pattern_getStripeTargetIndex(this, pos);

   return UInt16Vec_at(&thisCast->stripeTargetIDs, targetIndex);
}

void Raid0Pattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs)
{
   Raid0Pattern* thisCast = (Raid0Pattern*)this;

   ListTk_copyUInt16ListToVec( (UInt16List*)&thisCast->stripeTargetIDs, outTargetIDs);
}

UInt16Vec* Raid0Pattern_getStripeTargetIDs(StripePattern* this)
{
   Raid0Pattern* thisCast = (Raid0Pattern*)this;

   return &thisCast->stripeTargetIDs;
}

unsigned Raid0Pattern_getMinNumTargets(StripePattern* this)
{
   return 1;
}

unsigned Raid0Pattern_getDefaultNumTargets(StripePattern* this)
{
   Raid0Pattern* thisCast = (Raid0Pattern*)this;

   return thisCast->defaultNumTargets;
}


