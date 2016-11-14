#include "RefreshSessionMsg.h"


bool RefreshSessionMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // sessionID
   
   unsigned sessionBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &sessionIDLen, &sessionID, &sessionBufLen) )
      return false;
   
   bufPos += sessionBufLen;
   
   return true;   
}

void RefreshSessionMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // sessionID
   bufPos += Serialization::serializeStr(&buf[bufPos], sessionIDLen, sessionID);
}


