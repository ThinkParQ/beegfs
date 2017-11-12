#include "CreateEmptyContDirsMsg.h"

bool CreateEmptyContDirsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   //dirIDs
   if (!Serialization::deserializeStringListPreprocess(&buf[bufPos], bufLen-bufPos,
      &dirIDsElemNum, &dirIDsStart, &dirIDsBufLen))
      return false;

   bufPos += dirIDsBufLen;

   return true;
}

void CreateEmptyContDirsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // dirIDs
   bufPos += Serialization::serializeStringList(&buf[bufPos], dirIDs);
}
