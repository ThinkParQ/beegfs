#include "CloseChunkFileMsg.h"


bool CloseChunkFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // sessionID
   
   unsigned sessionBufLen;
   
   if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
      &sessionIDLen, &sessionID, &sessionBufLen) )
      return false;
   
   bufPos += sessionBufLen;
   
   // fileHandleID
   
   unsigned handleBufLen;
   
   if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
      &fileHandleIDLen, &fileHandleID, &handleBufLen) )
      return false;
   
   bufPos += handleBufLen;

   // flags

   unsigned flagsBufLen;

   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
      &flags, &flagsBufLen) )
      return false;

   bufPos += flagsBufLen;
   
   // targetID

   unsigned targetBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &targetID, &targetBufLen) )
      return false;

   bufPos += targetBufLen;

   return true;   
}

void CloseChunkFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // sessionID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], sessionIDLen, sessionID);
   
   // fileHandleID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], fileHandleIDLen, fileHandleID);

   // flags
   bufPos += Serialization::serializeUInt(&buf[bufPos], flags);

   // targetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], targetID);
}

