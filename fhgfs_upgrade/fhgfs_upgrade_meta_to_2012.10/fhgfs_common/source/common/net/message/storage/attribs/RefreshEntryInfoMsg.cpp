#include "RefreshEntryInfoMsg.h"


bool RefreshEntryInfoMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // entryInfo

   unsigned entryBufLen;

   if (!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
      return false;

   bufPos += entryBufLen; // not needed right now. included to avoid human errors later ;)

   return true;
}

void RefreshEntryInfoMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // path

   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
}
