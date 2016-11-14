#include "FixInodeOwnersInDentryMsg.h"

bool FixInodeOwnersInDentryMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // dentries
   if (!SerializationFsck::deserializeFsckDirEntryListPreprocess(&buf[bufPos],bufLen-bufPos,
      &dentriesStart, &dentriesElemNum, &dentriesBufLen))
      return false;

   bufPos += dentriesBufLen;

   return true;
}

void FixInodeOwnersInDentryMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // dentries
   bufPos += SerializationFsck::serializeFsckDirEntryList(&buf[bufPos], dentries);
}
