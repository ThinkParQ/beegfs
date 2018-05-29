#include <common/toolkit/serialization/Serialization.h>
#include "BuddyMirrorPattern.h"
#include "Raid0Pattern.h"
#include "Raid10Pattern.h"
#include "StripePattern.h"

/**
 * @return outPattern; NULL on error
 */
StripePattern* StripePattern::deserialize(Deserializer& des, bool hasPoolId)
{
   StripePatternHeader header(hasPoolId);

   des % header;
   if(unlikely(!des.good() || !header.chunkSize) )
   {
      des.setBad();
      return NULL;
   }

   StripePattern* pattern = NULL;
   auto poolId = boost::make_optional(hasPoolId, header.storagePoolId);

   switch(header.type)
   {
      case StripePatternType_Raid0:
         pattern = new Raid0Pattern(header.chunkSize, poolId);
         break;

      case StripePatternType_Raid10:
         pattern = new Raid10Pattern(header.chunkSize, poolId);
         break;

      case StripePatternType_BuddyMirror:
         pattern = new BuddyMirrorPattern(header.chunkSize, poolId);
         break;

      case StripePatternType_Invalid:
         des.setBad();
         return NULL;
   }

   pattern->deserializeContent(des);

   if(unlikely(!des.good() ) )
   {
      LogContext(__PRETTY_FUNCTION__).logErr("Deserialization failed. "
         "PatternType: " + StringTk::uintToStr(header.type) );
      delete pattern;
      des.setBad();
      return NULL;
   }

   return pattern;
}

/**
 * Returns human-readable pattern type.
 * Just a convenience wrapper for the static version with the same name.
 */
std::string StripePattern::getPatternTypeStr() const
{
   return getPatternTypeStr(header.type);
}

/**
 * Returns human-readable pattern type.
 */
std::string StripePattern::getPatternTypeStr(StripePatternType patternType)
{
   switch(patternType)
   {
      case StripePatternType_Raid0:
         return "RAID0";

      case StripePatternType_Raid10:
         return "RAID10";

      case StripePatternType_BuddyMirror:
         return "Buddy Mirror";

      default:
         return "<unknown(" + StringTk::uintToStr(patternType) + ")>";
   }
}

bool StripePattern::stripePatternEquals(const StripePattern* second) const
{
   return getPatternType() == second->getPatternType()
      && getChunkSize() == second->getChunkSize()
      && getStoragePoolId() == second->getStoragePoolId()
      && patternEquals(second);
}
