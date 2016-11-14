#include "LookupIntentRespMsg.h"

bool LookupIntentRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // responseFlags
      unsigned responseFlagsBufLen;

      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &this->responseFlags, &responseFlagsBufLen) )
         return false;

      bufPos += responseFlagsBufLen;
   }

   { // lookupResult
      unsigned lookupResultBufLen;

      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &this->lookupResult, &lookupResultBufLen) )
         return false;

      bufPos += lookupResultBufLen;
   }

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
   {
      {  // statResult
         unsigned statResultFieldLen;

         if (!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
            &this->statResult, &statResultFieldLen) )
            return false;

         bufPos += statResultFieldLen;
      }

      { // statData
         unsigned statDataLen;

         if (!this->statData.deserialize(false, &buf[bufPos], bufLen-bufPos, &statDataLen) )
            return false;

         bufPos += statDataLen;
      }

   }

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE)
   {
      // revalidateResult
      unsigned revalidateResultFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &revalidateResult, &revalidateResultFieldLen) )
         return false;

      bufPos += revalidateResultFieldLen;
   }

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
   {
      // createResult
      unsigned createResultFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &createResult, &createResultFieldLen) )
         return false;

      bufPos += createResultFieldLen;
   }

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
   {
      // openResult
      unsigned openResultFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &openResult, &openResultFieldLen) )
         return false;

      bufPos += openResultFieldLen;

      // fileHandleID
      unsigned handleBufLen;
      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &fileHandleIDLen, &fileHandleID, &handleBufLen) )
         return false;

      bufPos += handleBufLen;

      // stripePattern
      unsigned patternBufLen;
      if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
         &patternHeader, &patternStart, &patternBufLen) )
         return false;

      bufPos += patternBufLen;
   }

   if (this->lookupResult == FhgfsOpsErr_SUCCESS || this->createResult == FhgfsOpsErr_SUCCESS)
   { // entryInfo, only deserialize it if either of it was successful and so we have those data
      unsigned entryLen;

      if(!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryLen) )
         return false;

      bufPos += entryLen;
   }


   return true;
}

void LookupIntentRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // responseFlags
   bufPos += Serialization::serializeInt(&buf[bufPos], responseFlags);

   // result
   bufPos += Serialization::serializeInt(&buf[bufPos], lookupResult);

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
   {
      // statResult
      bufPos += Serialization::serializeInt(&buf[bufPos], statResult);

      // statData
      bufPos += this->statData.serialize(false, &buf[bufPos]);
      // std::cerr << __func__ << ":" << __LINE__ << ": bufPos " << bufPos << std::endl;
   }

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE)
   {
      // revalidateResult
      bufPos += Serialization::serializeInt(&buf[bufPos], revalidateResult);
   }

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
   {
      // createResult
      bufPos += Serialization::serializeInt(&buf[bufPos], createResult);
   }

   if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
   {
      // openResult
      bufPos += Serialization::serializeInt(&buf[bufPos], openResult);

      // fileHandleID
      bufPos += Serialization::serializeStrAlign4(&buf[bufPos], fileHandleIDLen, fileHandleID);

      // stripePattern
      bufPos += pattern->serialize(&buf[bufPos]);
   }

   // std::cerr << __func__ << ":" << __LINE__ << " lookupResult: " << this->lookupResult
   //   << " createResult " << this->createResult << std::endl;

   if (this->lookupResult == FhgfsOpsErr_SUCCESS || this->createResult == FhgfsOpsErr_SUCCESS)
   {
      bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
   }

}

