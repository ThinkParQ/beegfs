#include "StatMsg.h"

bool StatMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // entryInfo
   unsigned entryBufLen;

   if(!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
      return false;

   bufPos += entryBufLen;

   return true;
}

void StatMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
}

