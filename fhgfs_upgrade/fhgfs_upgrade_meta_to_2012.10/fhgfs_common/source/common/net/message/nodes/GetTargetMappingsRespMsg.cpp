#include "GetTargetMappingsRespMsg.h"

void GetTargetMappingsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // targetIDs
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], targetIDs);

   // nodeIDs
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], nodeIDs);
}

bool GetTargetMappingsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // targetIDs

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
      &targetIDsElemNum, &targetIDsListStart, &targetIDsBufLen) )
      return false;

   bufPos += targetIDsBufLen;

   // nodeIDs

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
      &nodeIDsElemNum, &nodeIDsListStart, &nodeIDsBufLen) )
      return false;

   bufPos += nodeIDsBufLen;

   return true;
}

