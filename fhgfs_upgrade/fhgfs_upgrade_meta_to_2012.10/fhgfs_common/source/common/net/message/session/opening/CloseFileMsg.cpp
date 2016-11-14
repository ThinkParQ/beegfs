#include "CloseFileMsg.h"


bool CloseFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // sessionID
      unsigned sessionBufLen;
      const char* sessionIDChar;
      unsigned sessionIDLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &sessionIDLen, &sessionIDChar, &sessionBufLen) )
         return false;

      this->sessionID.assign(sessionIDChar, sessionIDLen);
      bufPos += sessionBufLen;
   }
   
   { // fileHandleID
      unsigned handleBufLen;
      const char* fileHandleIDChar;
      unsigned fileHandleIDLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &fileHandleIDLen, &fileHandleIDChar, &handleBufLen) )
         return false;

      this->fileHandleID.assign(fileHandleIDChar, fileHandleIDLen);
      bufPos += handleBufLen;
   }

   { // entryInfo
      unsigned entryLen;

      if (!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryLen) )
         return false;

      bufPos += entryLen;
   }

   { // maxUsedNodeIndex
      unsigned maxUsedBufLen;

      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &maxUsedNodeIndex, &maxUsedBufLen) )
         return false;

      bufPos += maxUsedBufLen;
   }

   return true;
}

void CloseFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // sessionID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      this->sessionID.length(), this->sessionID.c_str() );
   
   // fileHandleID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      this->fileHandleID.length(), this->fileHandleID.c_str() );

   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);

   // maxUsedNodeIndex
   bufPos += Serialization::serializeInt(&buf[bufPos], maxUsedNodeIndex);
}



