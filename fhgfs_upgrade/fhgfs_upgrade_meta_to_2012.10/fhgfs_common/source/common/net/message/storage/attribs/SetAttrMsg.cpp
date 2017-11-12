#include "SetAttrMsg.h"

void SetAttrMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // validAttribs
   bufPos += Serialization::serializeInt(&buf[bufPos], validAttribs);

   // mode
   bufPos += Serialization::serializeInt(&buf[bufPos], attribs.mode);

   // modificationTimeSecs
   bufPos += Serialization::serializeInt64(&buf[bufPos], attribs.modificationTimeSecs);

   // lastAccessTimeSecs
   bufPos += Serialization::serializeInt64(&buf[bufPos], attribs.lastAccessTimeSecs);

   // userID
   bufPos += Serialization::serializeUInt(&buf[bufPos], attribs.userID);

   // groupID
   bufPos += Serialization::serializeUInt(&buf[bufPos], attribs.groupID);

   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
}

bool SetAttrMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   {// validAttribs
      unsigned validAttribsFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &validAttribs,
         &validAttribsFieldLen) )
         return false;

      bufPos += validAttribsFieldLen;
   }

   { // mode
      unsigned modeFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &attribs.mode, &modeFieldLen) )
         return false;

      bufPos += modeFieldLen;
   }

   { // modificationTimeSecs
      unsigned modificationTimeSecsLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &attribs.modificationTimeSecs,
         &modificationTimeSecsLen) )
         return false;

      bufPos += modificationTimeSecsLen;
   }

   { // lastAccessTimeSecs
      unsigned lastAccessTimeSecsLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &attribs.lastAccessTimeSecs,
         &lastAccessTimeSecsLen) )
         return false;

      bufPos += lastAccessTimeSecsLen;
   }

   { // userID
      unsigned userIDFieldLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &attribs.userID,
         &userIDFieldLen) )
         return false;

      bufPos += userIDFieldLen;
   }

   { // groupID
      unsigned groupIDFieldLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &attribs.groupID,
         &groupIDFieldLen) )
         return false;

      bufPos += groupIDFieldLen;
   }

   { // entryInfo
      unsigned entryBufLen;
      this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen);

      bufPos += entryBufLen;
   }

   return true;
}


