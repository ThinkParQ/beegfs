#include "RecreateDentriesMsg.h"

bool RecreateDentriesMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // fsIDs
   unsigned fsIDsBufLen;

   if ( !SerializationFsck::deserializeFsckObjectListPreprocess<FsckFsID>(&buf[bufPos],
      bufLen - bufPos, &fsIDsStart, &fsIDsElemNum, &fsIDsBufLen) )
      return false;

   bufPos += fsIDsBufLen;

   return true;
}

void RecreateDentriesMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // fsIDs
   bufPos += SerializationFsck::serializeFsckObjectList(&buf[bufPos], fsIDs);
}
