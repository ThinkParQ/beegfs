#include "FLockEntryMsg.h"


bool FLockEntryMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // clientFD
      unsigned clientFDLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &clientFD, &clientFDLen) )
         return false;

      bufPos += clientFDLen;
   }

   { // ownerPID
      unsigned ownerPIDLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &ownerPID, &ownerPIDLen) )
         return false;

      bufPos += ownerPIDLen;
   }

   { // lockTypeFlags
      unsigned lockTypeFlagsLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &lockTypeFlags, &lockTypeFlagsLen) )
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

void FLockEntryMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // clientFD
   bufPos += Serialization::serializeInt64(&buf[bufPos], clientFD);

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

