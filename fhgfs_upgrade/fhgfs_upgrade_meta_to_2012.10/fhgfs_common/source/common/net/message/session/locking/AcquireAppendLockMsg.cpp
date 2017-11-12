#include "AcquireAppendLockMsg.h"


bool AcquireAppendLockMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // sessionID
   
   unsigned strBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &sessionIDLen, &sessionID, &strBufLen) )
      return false;
   
   bufPos += strBufLen;
   
   // fd

   unsigned fdLen;
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &fd, &fdLen) )
      return false;
   
   bufPos += fdLen;
   
   return true;   
}

void AcquireAppendLockMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // sessionID
   
   bufPos += Serialization::serializeStr(&buf[bufPos], sessionIDLen, sessionID);
   
   // fd
   
   bufPos += Serialization::serializeInt(&buf[bufPos], fd);
}

