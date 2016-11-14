#include "OpenFileMsg.h"


bool OpenFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   {// accessFlags
      unsigned accessFlagsLen;
   
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &accessFlags, &accessFlagsLen) )
         return false;

      bufPos += accessFlagsLen;
   }

   {// sessionID
      unsigned sessionBufLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &sessionIDLen, &sessionID, &sessionBufLen) )
         return false;

      bufPos += sessionBufLen;
   }
   
   {// entryInfo
      unsigned entryBufLen;

      if (!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
         return false;

      bufPos += entryBufLen;
   }
   
   return true;
}

void OpenFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // accessFlags
   bufPos += Serialization::serializeUInt(&buf[bufPos], accessFlags);

   // sessionID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], sessionIDLen, sessionID);
   
   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
}


