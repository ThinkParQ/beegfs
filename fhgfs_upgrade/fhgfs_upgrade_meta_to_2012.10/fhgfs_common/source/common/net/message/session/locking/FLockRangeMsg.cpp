#include "FLockRangeMsg.h"


bool FLockRangeMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // start
      unsigned startLen;
      if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos, &start, &startLen) )
         return false;

      bufPos += startLen;
   }

   { // end
      unsigned endLen;
      if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos, &end, &endLen) )
         return false;

      bufPos += endLen;
   }

   { // ownerPID
      unsigned ownerPIDLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &ownerPID, &ownerPIDLen) )
         return false;

      bufPos += ownerPIDLen;
   }

   { // lockTypeFlags
      unsigned lockTypeFlagsLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &lockTypeFlags, &lockTypeFlagsLen) )
         return false;

      bufPos += lockTypeFlagsLen;
   }

   { // entryInfo
      unsigned entryBufLen;

      if (!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
         return false;

      bufPos += entryBufLen;
   }

   { // clientID
      unsigned clientBufLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &clientIDLen, &clientID, &clientBufLen) )
         return false;

      bufPos += clientBufLen;
   }

   { // fileHandleID
      unsigned fileHandleBufLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &fileHandleIDLen, &fileHandleID, &fileHandleBufLen) )
         return false;

      bufPos += fileHandleBufLen;
   }

   { // lockAckID
      unsigned lockAckBufLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &lockAckIDLen, &lockAckID, &lockAckBufLen) )
         return false;

      bufPos += lockAckBufLen;
   }

   return true;
}

void FLockRangeMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // start
   bufPos += Serialization::serializeUInt64(&buf[bufPos], start);

   // end
   bufPos += Serialization::serializeUInt64(&buf[bufPos], end);

   // ownerPID
   bufPos += Serialization::serializeInt(&buf[bufPos], ownerPID);

   // lockTypeFlags
   bufPos += Serialization::serializeInt(&buf[bufPos], lockTypeFlags);

   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);

   // clientID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], clientIDLen, clientID);

   // fileHandleID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], fileHandleIDLen, fileHandleID);

   // lockAckID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], lockAckIDLen, lockAckID);
}


