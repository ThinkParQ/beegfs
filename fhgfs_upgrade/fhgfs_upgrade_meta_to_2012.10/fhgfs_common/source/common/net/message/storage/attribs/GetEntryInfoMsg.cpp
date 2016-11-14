#include "GetEntryInfoMsg.h"

bool GetEntryInfoMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // entryInfo

   unsigned entryInfoBufLen; 
   
   if (!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryInfoBufLen) )
      return false;

   bufPos += entryInfoBufLen; // not needed right now. included to avoid human errors later ;)
   
   return true;
}

void GetEntryInfoMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // path
   
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
}

