#include "Raid0Pattern.h"


void Raid0Pattern::serializePattern(char* buf)
{
   size_t bufPos = 0;
   
   // stripeTargetIDs
   bufPos += Serialization::serializeStringVec(&buf[bufPos], &stripeTargetIDs);
   
   // defaultNumTargets
   bufPos += Serialization::serializeUInt(&buf[bufPos], defaultNumTargets);

}


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
      targetIDsLen, targetIDsElemNum, targetIDsListStart, &stripeTargetIDs) )
      return false;
   
   
   // defaultNumTargets
   unsigned defNumInfoFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
      &defaultNumTargets, &defNumInfoFieldLen) )
      return false;
      
   bufPos += defNumInfoFieldLen;
   
   
   // calc stripeSetSize
   stripeSetSize = stripeTargetIDs.size() * getChunkSize();
   
   
   // note: empty stripeTargetIDs is not an error, because directories always don't have those.
   
   return true;
}




