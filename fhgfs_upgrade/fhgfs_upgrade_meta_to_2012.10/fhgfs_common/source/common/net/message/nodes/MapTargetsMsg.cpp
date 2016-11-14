#include "MapTargetsMsg.h"

void MapTargetsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // targetIDs
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], targetIDs);

   // nodeID
   bufPos += Serialization::serializeUShort(&buf[bufPos], nodeID);

   // ackID
   bufPos += Serialization::serializeStr(&buf[bufPos], ackIDLen, ackID);
}

bool MapTargetsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // targetIDs

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
      &targetIDsElemNum, &targetIDsListStart, &targetIDsBufLen) )
      return false;

   bufPos += targetIDsBufLen;

   // nodeID

   unsigned idBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &nodeID, &idBufLen) )
      return false;

   bufPos += idBufLen;

   // ackID

   unsigned ackBufLen;

   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &ackIDLen, &ackID, &ackBufLen) )
      return false;

   bufPos += ackBufLen;

   return true;
}



