#include "GetNodesRespMsg.h"

bool GetNodesRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // rootNumID
   
   unsigned rootNumIDBufLen;
   
   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &rootNumID, &rootNumIDBufLen) )
      return false;
   
   bufPos += rootNumIDBufLen;

   // nodeList

   unsigned nodeListBufLen;
   
   if(!Serialization::deserializeNodeListPreprocess(&buf[bufPos], bufLen-bufPos,
      &nodeListElemNum, &nodeListStart, &nodeListBufLen) )
      return false;
   
   bufPos += nodeListBufLen;
   
   return true;   
}

void GetNodesRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // rootNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], rootNumID);

   // nodeList
   bufPos += Serialization::serializeNodeList(&buf[bufPos], nodeList);
}



