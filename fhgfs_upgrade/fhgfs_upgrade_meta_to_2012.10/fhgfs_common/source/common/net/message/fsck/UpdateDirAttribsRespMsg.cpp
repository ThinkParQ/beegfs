#include "UpdateDirAttribsRespMsg.h"

bool UpdateDirAttribsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // failedInodes
   if (!SerializationFsck::deserializeFsckDirInodeListPreprocess(&buf[bufPos],bufLen-bufPos,
      &failedInodesStart, &failedInodesElemNum, &failedInodesBufLen))
      return false;

   bufPos += failedInodesBufLen;

   return true;
}

void UpdateDirAttribsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // failedInodes
   bufPos += SerializationFsck::serializeFsckDirInodeList(&buf[bufPos], failedInodes);
}
