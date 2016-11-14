#include "GetHostByNameMsg.h"

bool GetHostByNameMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // hostname
   
   unsigned hostnameBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &hostnameLen, &hostname, &hostnameBufLen) )
      return false;
   
   bufPos += hostnameBufLen;

   return true;   
}

void GetHostByNameMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // hostname
   bufPos += Serialization::serializeStr(&buf[bufPos], hostnameLen, hostname);
}



