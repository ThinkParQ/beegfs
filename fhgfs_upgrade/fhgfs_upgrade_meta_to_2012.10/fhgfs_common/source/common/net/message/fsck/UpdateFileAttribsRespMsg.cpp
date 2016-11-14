#include "UpdateFileAttribsRespMsg.h"

bool UpdateFileAttribsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // failedInodes
   if (!SerializationFsck::deserializeFsckFileInodeListPreprocess(&buf[bufPos],bufLen-bufPos,
      &failedInodesStart, &failedInodesElemNum, &failedInodesBufLen))
      return false;

   bufPos += failedInodesBufLen;

   return true;
}

void UpdateFileAttribsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // failedInodes
   bufPos += SerializationFsck::serializeFsckFileInodeList(&buf[bufPos], failedInodes);
}
