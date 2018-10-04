#include <common/toolkit/Serialization.h>
#include "BuddyMirrorPattern.h"
#include "Raid0Pattern.h"
#include "Raid10Pattern.h"
#include "SimplePattern.h"
#include "StripePattern.h"

#define HAS_NO_POOL_FLAG (1 << 24)

/**
 * Calls the virtual uninit method and kfrees the object.
 */
void StripePattern_virtualDestruct(StripePattern* this)
{
   this->uninit(this);
   kfree(this);
}


bool StripePattern_deserializePatternPreprocess(DeserializeCtx* ctx,
   const char** outPatternStart, uint32_t* outPatternLength)
{
   DeserializeCtx temp = *ctx;

   if(!Serialization_deserializeUInt(&temp, outPatternLength))
      return false;

   *outPatternStart = ctx->data;

   if (*outPatternLength > ctx->length)
      return false;

   ctx->data += *outPatternLength;
   ctx->length -= *outPatternLength;

   return true;
}

/**
 * @return outPattern; outPattern->patternType is STRIPEPATTERN_Invalid on error
 */
StripePattern* StripePattern_createFromBuf(const char* patternStart,
   uint32_t patternLength)
{
   struct StripePatternHeader patternHeader;
   StripePattern* pattern;
   DeserializeCtx ctx = {
      .data = patternStart,
      .length = patternLength,
   };
   bool deserRes;

   if (!__StripePattern_deserializeHeader(&ctx, &patternHeader))
      return (StripePattern*)SimplePattern_construct(STRIPEPATTERN_Invalid, 0);

   switch (patternHeader.patternType)
   {
      case STRIPEPATTERN_Raid0:
      {
         pattern = (StripePattern*)Raid0Pattern_constructFromChunkSize(patternHeader.chunkSize);
      } break;

      case STRIPEPATTERN_Raid10:
      {
         pattern = (StripePattern*)Raid10Pattern_constructFromChunkSize(patternHeader.chunkSize);
      } break;

      case STRIPEPATTERN_BuddyMirror:
      {
         pattern = (StripePattern*)BuddyMirrorPattern_constructFromChunkSize(
            patternHeader.chunkSize);
      } break;

      default:
      {
         pattern = (StripePattern*)SimplePattern_construct(STRIPEPATTERN_Invalid, 0);
         return pattern;
      } break;
   }

   deserRes = pattern->deserializePattern(pattern, &ctx);
   if(unlikely(!deserRes) )
   { // deserialization failed => discard half-initialized pattern and create new invalid pattern
      StripePattern_virtualDestruct(pattern);

      pattern = (StripePattern*)SimplePattern_construct(STRIPEPATTERN_Invalid, 0);

      return pattern;
   }

   return pattern;

}


bool __StripePattern_deserializeHeader(DeserializeCtx* ctx,
   struct StripePatternHeader* outPatternHeader)
{
   // pattern length
   if(!Serialization_deserializeUInt(ctx, &outPatternHeader->patternLength) )
      return false;

   // pattern type
   if(!Serialization_deserializeUInt(ctx, &outPatternHeader->patternType) )
      return false;

   // chunkSize
   if(!Serialization_deserializeUInt(ctx, &outPatternHeader->chunkSize) )
      return false;

   // storagePoolId
   if (!(outPatternHeader->patternType & HAS_NO_POOL_FLAG)) {
      if(!StoragePoolId_deserialize(ctx, &outPatternHeader->storagePoolId) )
         return false;
   }

   outPatternHeader->patternType &= ~HAS_NO_POOL_FLAG;

   // check length field
   if(outPatternHeader->patternLength < STRIPEPATTERN_HEADER_LENGTH)
      return false;

   // check chunkSize
   if(!outPatternHeader->chunkSize)
      return false;

   return true;
}

/**
 * Predefined virtual method returning NULL. Will be overridden by StripePatterns (e.g. Raid10)
 * that actually do have mirror targets.
 *
 * @return NULL for patterns that don't have mirror targets.
 */
UInt16Vec* StripePattern_getMirrorTargetIDs(StripePattern* this)
{
   return NULL;
}
