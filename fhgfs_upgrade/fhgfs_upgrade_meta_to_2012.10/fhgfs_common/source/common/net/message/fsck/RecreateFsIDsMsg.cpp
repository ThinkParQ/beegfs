#include "RecreateFsIDsMsg.h"

bool RecreateFsIDsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   //entryIDs
   if (!SerializationFsck::deserializeFsckDirEntryListPreprocess(&buf[bufPos],bufLen-bufPos,
      &entriesStart, &entriesElemNum, &entriesBufLen))
      return false;

   bufPos += entriesBufLen;

   return true;
}

void RecreateFsIDsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // entryIDs
   bufPos += SerializationFsck::serializeFsckDirEntryList(&buf[bufPos], entries);
}
