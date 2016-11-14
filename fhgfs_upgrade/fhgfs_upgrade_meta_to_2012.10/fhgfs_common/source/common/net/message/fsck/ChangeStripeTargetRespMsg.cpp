#include "ChangeStripeTargetRespMsg.h"

bool ChangeStripeTargetRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // failedInodes
   if (!SerializationFsck::deserializeFsckObjectListPreprocess<FsckFileInode>(&buf[bufPos],
      bufLen-bufPos, &failedInodesStart, &failedInodesElemNum, &failedInodesBufLen))
      return false;

   bufPos += failedInodesBufLen;

   return true;
}

void ChangeStripeTargetRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // failedInodes
   bufPos += SerializationFsck::serializeFsckObjectList(&buf[bufPos], this->failedInodes);
}
