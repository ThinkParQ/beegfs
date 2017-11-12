#include "RmLocalDirMsg.h"

bool RmLocalDirMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // delEntryInfo
      unsigned entryBufLen;

      if(!this->delEntryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
         return false;

      bufPos += entryBufLen;
   }

   return true;
}

void RmLocalDirMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   bufPos += this->delEntryInfoPtr->serialize(&buf[bufPos]);
}

