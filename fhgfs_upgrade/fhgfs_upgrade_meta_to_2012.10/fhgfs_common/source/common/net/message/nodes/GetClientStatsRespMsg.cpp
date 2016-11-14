/*
 * GetClientStatsRespMsg.cpp
 *
 */

#include "GetClientStatsRespMsg.h"

/**
 * Used on the fhgfs-ctl side to decode stats messages send by servers
 */
bool GetClientStatsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // We only call preprocess here

   if(!Serialization::deserializeUInt64VectorPreprocess(&buf[bufPos], bufLen - bufPos,
         &this->statsVecElemNum, &this->statsVecStart, &this->statsVecBufLen) )
      return false;

   bufPos += statsVecBufLen; // just to avoid human errors on possible additions later on

   return true;
}

/**
 * Used on the server side to encode stats messages
 */
void GetClientStatsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // statsVector
   bufPos += Serialization::serializeUInt64Vector(&buf[bufPos], statsVec);
}

