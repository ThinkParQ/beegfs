#include "RecreateFsIDsRespMsg.h"

bool RecreateFsIDsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // failedDentries
   if (!SerializationFsck::deserializeFsckDirEntryListPreprocess(&buf[bufPos],bufLen-bufPos,
      &failedEntriesStart, &failedEntriesElemNum, &failedEntriesBufLen))
      return false;

   bufPos += failedEntriesBufLen;

   return true;
}

void RecreateFsIDsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // entryIDs
   bufPos += SerializationFsck::serializeFsckDirEntryList(&buf[bufPos], failedEntries);
}
