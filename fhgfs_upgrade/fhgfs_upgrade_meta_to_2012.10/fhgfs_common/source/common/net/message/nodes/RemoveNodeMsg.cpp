#include "RemoveNodeMsg.h"

bool RemoveNodeMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // nodeType

   unsigned nodeTypeBufLen;

   if(!Serialization::deserializeShort(&buf[bufPos], bufLen-bufPos,
      &nodeType, &nodeTypeBufLen) )
      return false;

   bufPos += nodeTypeBufLen;

   // nodeNumID

   unsigned nodeNumIDBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &nodeNumID, &nodeNumIDBufLen) )
      return false;

   bufPos += nodeNumIDBufLen;

   // nodeID
   
   unsigned strBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &nodeIDLen, &nodeID, &strBufLen) )
      return false;
   
   bufPos += strBufLen;
   
   // ackID
   
   unsigned ackBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &ackIDLen, &ackID, &ackBufLen) )
      return false;
   
   bufPos += ackBufLen;

   return true;
}

void RemoveNodeMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // nodeType
   bufPos += Serialization::serializeShort(&buf[bufPos], nodeType);

   // nodeNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], nodeNumID);

   // nodeID
   bufPos += Serialization::serializeStr(&buf[bufPos], nodeIDLen, nodeID);
   
   // ackID
   bufPos += Serialization::serializeStr(&buf[bufPos], ackIDLen, ackID);

}

