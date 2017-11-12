#include "RegisterTargetMsg.h"

bool RegisterTargetMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // targetID
      unsigned targetBufLen;

      if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
         &targetIDLen, &targetID, &targetBufLen) )
         return false;

      bufPos += targetBufLen;
   }

   { // targetNumID
      unsigned targetNumIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &targetNumID, &targetNumIDBufLen) )
         return false;

      bufPos += targetNumIDBufLen;
   }

   return true;
}

void RegisterTargetMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // targetID
   bufPos += Serialization::serializeStr(&buf[bufPos], targetIDLen, targetID);

   // targetNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], targetNumID);
}

