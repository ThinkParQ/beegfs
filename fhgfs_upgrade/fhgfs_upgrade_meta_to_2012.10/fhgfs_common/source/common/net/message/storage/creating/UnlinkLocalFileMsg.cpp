#include "UnlinkLocalFileMsg.h"

bool UnlinkLocalFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
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

void UnlinkLocalFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // targetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], targetID);

   // mirroredFromTargetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], mirroredFromTargetID);

   // entryID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], entryIDLen, entryID);

}


