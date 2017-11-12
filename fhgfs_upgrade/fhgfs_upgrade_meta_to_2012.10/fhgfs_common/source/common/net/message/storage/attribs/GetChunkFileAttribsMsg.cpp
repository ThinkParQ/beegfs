#include "GetChunkFileAttribsMsg.h"

bool GetChunkFileAttribsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // entryID

   unsigned entryBufLen;

   if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
      &entryIDLen, &entryID, &entryBufLen) )
      return false;

   bufPos += entryBufLen;

   // targetID

   unsigned targetBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &targetID, &targetBufLen) )
      return false;

   bufPos += targetBufLen;

   return true;
}

void GetChunkFileAttribsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // entryID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], entryIDLen, entryID);

   // targetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], targetID);
}

