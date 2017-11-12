#include <common/toolkit/Serialization.h>
#include "BuddyMirrorPattern.h"
#include "Raid0Pattern.h"
#include "Raid10Pattern.h"
#include "SimplePattern.h"
#include "StripePattern.h"


/**
 * Calls the virtual uninit method and kfrees the object.
 */
void StripePattern_virtualDestruct(StripePattern* this)
{
   this->uninit(this);
   kfree(this);
}

/**
 * @return outPattern; outPattern->patternType is STRIPEPATTERN_Invalid on error
 */
StripePattern* StripePattern_createFromBuf(const char* patternStart,
   struct StripePatternHeader* patternHeader)
{
   StripePattern* pattern;
   DeserializeCtx ctx = {
      .data = patternStart + STRIPEPATTERN_HEADER_LENGTH,
      .length = patternHeader->patternLength - STRIPEPATTERN_HEADER_LENGTH,
   };
   bool deserRes;

   switch(patternHeader->patternType)
   {
      case STRIPEPATTERN_Raid0:
      {
         pattern = (StripePattern*)Raid0Pattern_constructFromChunkSize(patternHeader->chunkSize);
      } break;

      case STRIPEPATTERN_Raid10:
      {
         pattern = (StripePattern*)Raid10Pattern_constructFromChunkSize(patternHeader->chunkSize);
      } break;

      case STRIPEPATTERN_BuddyMirror:
      {
         pattern = (StripePattern*)BuddyMirrorPattern_constructFromChunkSize(
            patternHeader->chunkSize);
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


bool __StripePattern_deserializeHeader(DeserializeCtx ctx,
   struct StripePatternHeader* outPatternHeader)
{
   // pattern length
   if(!Serialization_deserializeUInt(&ctx, &outPatternHeader->patternLength) )
      return false;

   // pattern type
   if(!Serialization_deserializeUInt(&ctx, &outPatternHeader->patternType) )
      return false;

   // chunkSize
   if(!Serialization_deserializeUInt(&ctx, &outPatternHeader->chunkSize) )
      return false;

   // storagePoolId
   if(!StoragePoolId_deserialize(&ctx, &outPatternHeader->storagePoolId) )
      return false;

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

/**
 * Returns human-readable pattern type.
 *
 * @return static string (not alloced => don't free it)
 */
const char* StripePattern_getPatternTypeStr(StripePattern* this)
{
   switch(this->patternType)
   {
      case STRIPEPATTERN_Invalid:
      {
         return "<invalid>";
      } break;

      case STRIPEPATTERN_Raid0:
      {
         return "RAID0";
      } break;

      case STRIPEPATTERN_Raid10:
      {
         return "RAID10";
      } break;

      default:
      {
         return "<unknown>";
      } break;
   }
}
