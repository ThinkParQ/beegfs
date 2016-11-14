#include <common/app/log/LogContext.h>
#include "Raid0Pattern.h"


void Raid0Pattern::serializePattern(char* buf)
{
   size_t bufPos = 0;
   
   // defaultNumTargets
   bufPos += Serialization::serializeUInt(&buf[bufPos], defaultNumTargets);

   // stripeTargetIDs
   bufPos += Serialization::serializeUInt16Vector(&buf[bufPos], &stripeTargetIDs);
}


/**
 * 2011.04 pattern deserialization
 */
bool Raid0Pattern::deserializePattern(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // targetIDs
   unsigned targetIDsLen;
   unsigned targetIDsElemNum;
   const char* targetIDsListStart;
   if(!Serialization::deserializeStringVecPreprocess(&buf[bufPos], bufLen-bufPos,
      &targetIDsElemNum, &targetIDsListStart, &targetIDsLen) )
      return false;
      
   bufPos += targetIDsLen;
   
   if(!Serialization::deserializeStringVec(
      targetIDsLen, targetIDsElemNum, targetIDsListStart, &stripeTargetIDsStrVec) )
      return false;
   
   
   // defaultNumTargets
   unsigned defNumInfoFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
      &defaultNumTargets, &defNumInfoFieldLen) )
      return false;

   bufPos += defNumInfoFieldLen;


   // Note: Do not forget to call later on mapStringIDs()

   // calc stripeSetSize
   stripeSetSize = stripeTargetIDs.size() * getChunkSize();
   
   
   // note: empty stripeTargetIDs is not an error, because directories always don't have those.
   
   return true;
}

bool Raid0Pattern::updateStripeTargetIDs(StripePattern* updatedStripePattern)
{
   if ( !updatedStripePattern )
      return false;

   if ( this->stripeTargetIDs.size() != updatedStripePattern->getStripeTargetIDs()->size() )
      return false;

   for ( unsigned i = 0; i < stripeTargetIDs.size(); i++ )
   {
      stripeTargetIDs[i] = updatedStripePattern->getStripeTargetIDs()->at(i);
   }

   return true;
}

bool Raid0Pattern::mapStringIDs(StringUnsignedMap* idMap)
{
   StringVectorIter vecIter;
   StringVector* vec = &this->stripeTargetIDsStrVec;

   for (vecIter = vec->begin(); vecIter != vec->end(); vecIter++)
   {
      StringUnsignedMapIter mapIter = idMap->find(*vecIter);
      if (unlikely(mapIter == idMap->end() ) )
      {
         LogContext("map string ID").logErr("");
         LogContext("map string ID").logErr("Could not map targetID-string to numeric ID: " +
            *vecIter);

         return false; // not in map
      }

      this->stripeTargetIDs.push_back((short)mapIter->second);
   }

   // calc stripeSetSize
   stripeSetSize = stripeTargetIDs.size() * getChunkSize();

   return true;
}



