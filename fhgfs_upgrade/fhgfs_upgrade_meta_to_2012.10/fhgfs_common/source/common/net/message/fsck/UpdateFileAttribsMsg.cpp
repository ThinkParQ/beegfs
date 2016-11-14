#include "UpdateFileAttribsMsg.h"

bool UpdateFileAttribsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // inodes
   if (!SerializationFsck::deserializeFsckFileInodeListPreprocess(&buf[bufPos],bufLen-bufPos,
      &inodesStart, &inodesElemNum, &inodesBufLen))
      return false;

   bufPos += inodesBufLen;

   return true;
}

void UpdateFileAttribsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // inodes
   bufPos += SerializationFsck::serializeFsckFileInodeList(&buf[bufPos], inodes);
}
