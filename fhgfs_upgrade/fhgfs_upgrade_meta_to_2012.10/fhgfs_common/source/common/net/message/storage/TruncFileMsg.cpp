#include "TruncFileMsg.h"


bool TruncFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   {// filesize
      unsigned filesizeLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &filesize, &filesizeLen) )
         return false;

      bufPos += filesizeLen;
   }

   { // entryInfo
      unsigned entryInfoBufLen;

      if(!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryInfoBufLen) )
         return false;

      bufPos += entryInfoBufLen;
   }

   return true;
}

void TruncFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // filesize
   bufPos += Serialization::serializeInt64(&buf[bufPos], filesize);

   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
}


