#include "ChangeStripeTargetMsg.h"

bool ChangeStripeTargetMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // inodes
   if ( !SerializationFsck::deserializeFsckObjectListPreprocess<FsckFileInode>(&buf[bufPos],
      bufLen - bufPos, &inodesStart, &inodesElemNum, &inodesBufLen) )
      return false;

   bufPos += inodesBufLen;

   // targetID
   unsigned targetIDBufLen;
   if (!Serialization::deserializeUShort(&buf[bufPos],bufLen-bufPos,
      &targetID, &targetIDBufLen))
      return false;

   bufPos += targetIDBufLen;

   // newTargetID
   unsigned newTargetIDBufLen;
   if (!Serialization::deserializeUShort(&buf[bufPos],bufLen-bufPos,
      &newTargetID, &newTargetIDBufLen))
      return false;

   bufPos += newTargetIDBufLen;

   return true;
}

void ChangeStripeTargetMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // inodes
   bufPos += SerializationFsck::serializeFsckObjectList(&buf[bufPos], inodes);

   // targetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], targetID);

   // newTargetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], newTargetID);
}
