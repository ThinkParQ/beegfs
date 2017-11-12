#include "Raid10Pattern.h"


void Raid10Pattern::serializePattern(char* buf)
{
   size_t bufPos = 0;

   // defaultNumTargets
   bufPos += Serialization::serializeUInt(&buf[bufPos], defaultNumTargets);

   // stripeTargetIDs
   bufPos += Serialization::serializeUInt16Vector(&buf[bufPos], &stripeTargetIDs);

   // mirrorTargetIDs
   bufPos += Serialization::serializeUInt16Vector(&buf[bufPos], &mirrorTargetIDs);
}


bool Raid10Pattern::deserializePattern(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // defaultNumTargets
   unsigned defNumInfoFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
      &defaultNumTargets, &defNumInfoFieldLen) )
      return false;

   bufPos += defNumInfoFieldLen;

   // stripeTargetIDs
   unsigned targetIDsLen;
   unsigned targetIDsElemNum;
   const char* targetIDsListStart;

   if(!Serialization::deserializeUInt16VectorPreprocess(&buf[bufPos], bufLen-bufPos,
      &targetIDsElemNum, &targetIDsListStart, &targetIDsLen) )
      return false;

   bufPos += targetIDsLen;

   if(!Serialization::deserializeUInt16Vector(
      targetIDsLen, targetIDsElemNum, targetIDsListStart, &stripeTargetIDs) )
      return false;

   // mirrorTargetIDs
   unsigned mirrorTargetIDsLen;
   unsigned mirrorTargetIDsElemNum;
   const char* mirrorTargetIDsListStart;

   if(!Serialization::deserializeUInt16VectorPreprocess(&buf[bufPos], bufLen-bufPos,
      &mirrorTargetIDsElemNum, &mirrorTargetIDsListStart, &mirrorTargetIDsLen) )
      return false;

   bufPos += mirrorTargetIDsLen;

   if(!Serialization::deserializeUInt16Vector(
      mirrorTargetIDsLen, mirrorTargetIDsElemNum, mirrorTargetIDsListStart, &mirrorTargetIDs) )
      return false;


   // calc stripeSetSize
   stripeSetSize = stripeTargetIDs.size() * getChunkSize();


   // note: empty stripeTargetIDs is not an error, because directories always don't have those.

   return true;
}

bool Raid10Pattern::updateStripeTargetIDs(StripePattern* updatedStripePattern)
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



