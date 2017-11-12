#include "CreateEmptyContDirsRespMsg.h"

bool CreateEmptyContDirsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   //failedDirIDs
   if (!Serialization::deserializeStringListPreprocess(&buf[bufPos], bufLen-bufPos,
      &failedDirIDsElemNum, &failedDirIDsStart, &failedDirIDsBufLen))
      return false;

   bufPos += failedDirIDsBufLen;

   return true;
}

void CreateEmptyContDirsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // faileDirIDs
   bufPos += Serialization::serializeStringList(&buf[bufPos], failedDirIDs);
}
