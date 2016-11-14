#include "RefreshCapacityPoolsMsg.h"

void RefreshCapacityPoolsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // ackID
   bufPos += Serialization::serializeStr(&buf[bufPos], ackIDLen, ackID);
}

bool RefreshCapacityPoolsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // ackID

   unsigned ackBufLen;

   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &ackIDLen, &ackID, &ackBufLen) )
      return false;

   bufPos += ackBufLen;

   return true;
}



