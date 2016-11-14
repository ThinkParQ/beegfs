#include "GetHostByNameRespMsg.h"

bool GetHostByNameRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // hostAddr
   
   unsigned hostAddrBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &hostAddrLen, &hostAddr, &hostAddrBufLen) )
      return false;
   
   bufPos += hostAddrBufLen;

   return true;   
}

void GetHostByNameRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // hostAddr
   bufPos += Serialization::serializeStr(&buf[bufPos], hostAddrLen, hostAddr);
}



