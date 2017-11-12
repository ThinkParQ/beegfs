#include "CreateDefDirInodesMsg.h"

bool CreateDefDirInodesMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // inodeIDs
   if (!Serialization::deserializeStringListPreprocess(&buf[bufPos], bufLen-bufPos,
      &inodeIDsElemNum, &inodeIDsStart, &inodeIDsBufLen))
      return false;

   bufPos += inodeIDsBufLen;

   return true;
}

void CreateDefDirInodesMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // inodeIDs
   bufPos += Serialization::serializeStringList(&buf[bufPos], inodeIDs);
}
