#include "UpdateDirAttribsMsg.h"

bool UpdateDirAttribsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // inodes
   if (!SerializationFsck::deserializeFsckDirInodeListPreprocess(&buf[bufPos],bufLen-bufPos,
      &inodesStart, &inodesElemNum, &inodesBufLen))
      return false;

   bufPos += inodesBufLen;

   return true;
}

void UpdateDirAttribsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // inodes
   bufPos += SerializationFsck::serializeFsckDirInodeList(&buf[bufPos], inodes);
}
