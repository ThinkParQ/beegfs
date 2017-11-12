#include "GetNodeCapacityPoolsRespMsg.h"

bool GetNodeCapacityPoolsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // listNormal

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
      &listNormalElemNum, &listNormalStart, &listNormalBufLen) )
      return false;

   bufPos += listNormalBufLen;

   // listLow

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
      &listLowElemNum, &listLowStart, &listLowBufLen) )
      return false;

   bufPos += listLowBufLen;

   // listEmergency

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
      &listEmergencyElemNum, &listEmergencyStart, &listEmergencyBufLen) )
      return false;

   bufPos += listEmergencyBufLen;


   return true;
}

void GetNodeCapacityPoolsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // listNormal
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], listNormal);

   // listLow
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], listLow);

   // listEmergency
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], listEmergency);
}

