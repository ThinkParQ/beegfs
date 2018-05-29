#include <common/toolkit/MathTk.h>
#include "Raid10Pattern.h"

bool Raid10Pattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   RawList targetIDsList;
   RawList mirrorTargetIDsList;

   // defaultNumTargets
   if(!Serialization_deserializeUInt(ctx, &thisCast->defaultNumTargets) )
      return false;

   // targetIDs
   if(!Serialization_deserializeUInt16VecPreprocess(ctx, &targetIDsList) )
      return false;

   if(!Serialization_deserializeUInt16Vec(&targetIDsList, &thisCast->stripeTargetIDs) )
      return false;

   // mirrorTargetIDs
   if(!Serialization_deserializeUInt16VecPreprocess(ctx, &mirrorTargetIDsList) )
      return false;

   if(!Serialization_deserializeUInt16Vec(&mirrorTargetIDsList, &thisCast->mirrorTargetIDs) )
      return false;

   // calc stripeSetSize
   thisCast->stripeSetSize = UInt16Vec_length(
      &thisCast->stripeTargetIDs) * StripePattern_getChunkSize(this);

   return true;
}

size_t Raid10Pattern_getStripeTargetIndex(StripePattern* this, int64_t pos)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   /* the code below is an optimization (wrt division/modulo) of following the two lines:
         int64_t stripeSetInnerOffset = pos % thisCast->stripeSetSize;
         int64_t targetIndex = stripeSetInnerOffset / StripePattern_getChunkSize(this); */

   // note: do_div(n64, base32) assigns the result to n64 and returns the remainder!
   // (do_div is needed for 64bit division on 32bit archs)

   unsigned stripeSetSize = thisCast->stripeSetSize;

   int64_t stripeSetInnerOffset;
   unsigned chunkSize;
   size_t targetIndex;

   if(MathTk_isPowerOfTwo(stripeSetSize) )
   { // quick path => no modulo needed
      stripeSetInnerOffset = pos & (stripeSetSize - 1);
   }
   else
   { // slow path => modulo
      stripeSetInnerOffset = do_div(pos, thisCast->stripeSetSize);

      // warning: do_div modifies pos! (so do not use it afterwards within this method)
   }

   chunkSize = StripePattern_getChunkSize(this);

   // this is "a=b/c" written as "a=b>>log2(c)", because chunkSize is a power of two.
   targetIndex = (stripeSetInnerOffset >> MathTk_log2Int32(chunkSize) );

   return targetIndex;
}

uint16_t Raid10Pattern_getStripeTargetID(StripePattern* this, int64_t pos)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   size_t targetIndex = Raid10Pattern_getStripeTargetIndex(this, pos);

   return UInt16Vec_at(&thisCast->stripeTargetIDs, targetIndex);
}

void Raid10Pattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   ListTk_copyUInt16ListToVec( (UInt16List*)&thisCast->stripeTargetIDs, outTargetIDs);
}

UInt16Vec* Raid10Pattern_getStripeTargetIDs(StripePattern* this)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   return &thisCast->stripeTargetIDs;
}

UInt16Vec* Raid10Pattern_getMirrorTargetIDs(StripePattern* this)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   return &thisCast->mirrorTargetIDs;
}

unsigned Raid10Pattern_getMinNumTargets(StripePattern* this)
{
   return 2;
}

unsigned Raid10Pattern_getDefaultNumTargets(StripePattern* this)
{
   Raid10Pattern* thisCast = (Raid10Pattern*)this;

   return thisCast->defaultNumTargets;
}

