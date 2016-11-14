#include "TruncLocalFileMsg.h"


bool TruncLocalFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // filesize

   unsigned filesizeLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &filesize, &filesizeLen) )
      return false;
   
   bufPos += filesizeLen;

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

   // mirroredFromTargetID

   unsigned mirroredFromTargetIDBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &mirroredFromTargetID, &mirroredFromTargetIDBufLen) )
      return false;

   bufPos += mirroredFromTargetIDBufLen;

   // entryID

   unsigned entryBufLen;

   if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
      &entryIDLen, &entryID, &entryBufLen) )
      return false;

   bufPos += entryBufLen;
   
   return true;   
}

void TruncLocalFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // filesize
   bufPos += Serialization::serializeInt64(&buf[bufPos], filesize);
   
   // flags
   bufPos += Serialization::serializeUInt(&buf[bufPos], flags);

   // targetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], targetID);

   // mirroredFromTargetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], mirroredFromTargetID);

   // entryID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], entryIDLen, entryID);
}


