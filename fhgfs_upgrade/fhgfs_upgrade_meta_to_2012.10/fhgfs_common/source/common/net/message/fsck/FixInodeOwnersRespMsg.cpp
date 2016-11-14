#include "FixInodeOwnersRespMsg.h"

bool FixInodeOwnersRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   //failedDentries
   if (!SerializationFsck::deserializeFsckDirInodeListPreprocess(&buf[bufPos],bufLen-bufPos,
      &failedInodesStart, &failedInodesElemNum, &failedInodesBufLen))
      return false;

   bufPos += failedInodesBufLen;

   return true;
}

void FixInodeOwnersRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // entryIDs
   bufPos += SerializationFsck::serializeFsckDirInodeList(&buf[bufPos], failedInodes);
}
