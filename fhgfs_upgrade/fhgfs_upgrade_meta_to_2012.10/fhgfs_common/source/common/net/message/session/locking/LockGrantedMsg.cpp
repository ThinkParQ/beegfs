#include "LockGrantedMsg.h"

bool LockGrantedMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // lockAckID

   unsigned lockAckIDBufLen;

   if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
      &lockAckIDLen, &lockAckID, &lockAckIDBufLen) )
      return false;

   bufPos += lockAckIDBufLen;

   // ackID

   unsigned ackBufLen;

   if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
      &ackIDLen, &ackID, &ackBufLen) )
      return false;

   bufPos += ackBufLen;

   // granterNodeID

   unsigned granterNodeIDBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &granterNodeID, &granterNodeIDBufLen) )
      return false;

   bufPos += granterNodeIDBufLen;

   return true;
}

void LockGrantedMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // lockAckID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], lockAckIDLen, lockAckID);

   // ackID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], ackIDLen, ackID);

   // granterNodeID
   bufPos += Serialization::serializeUShort(&buf[bufPos], granterNodeID);
}

