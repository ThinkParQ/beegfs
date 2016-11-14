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

   // calc stripeSetSize
   thisCast->stripeSetSize = UInt16Vec_length(
      &thisCast->mirrorBuddyGroupIDs) * StripePattern_getChunkSize(this);

   // check mirrorBuddyGroupIDs
   if(!UInt16Vec_length(&thisCast->mirrorBuddyGroupIDs) )
      return false;

   return true;
}

size_t BuddyMirrorPattern_getStripeTargetIndex(StripePattern* this, int64_t pos)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;
   
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


