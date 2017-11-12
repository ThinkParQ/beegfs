#include "RemoveInodeMsg.h"

bool RemoveInodeMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // entryID
   {
      unsigned entryIDBufLen;

      Serialization::deserializeStr(&buf[bufPos], bufLen - bufPos, &entryIDLen, &entryID,
         &entryIDBufLen);

      bufPos += entryIDBufLen;
   }

   {
      // entryType
      unsigned entryTypeLen = 0;
      int intEntryType = 0;
      Serialization::deserializeInt(&buf[bufPos], bufLen - bufPos, &intEntryType, &entryTypeLen);

      entryType = (DirEntryType) intEntryType;

      bufPos += entryTypeLen; // not needed right now. included to avoid human errors later ;)
   }
   
   return true;
}

void RemoveInodeMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // entryID
   bufPos += Serialization::serializeStr(&buf[bufPos], entryIDLen, entryID);

   // entryType
   bufPos += Serialization::serializeInt(&buf[bufPos],(int)entryType);
}

