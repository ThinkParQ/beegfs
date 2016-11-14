#include "SetLocalAttrMsg.h"

void SetLocalAttrMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // modificationTimeSecs
   bufPos += Serialization::serializeInt64(&buf[bufPos], attribs.modificationTimeSecs);

   // lastAccessTimeSecs
   bufPos += Serialization::serializeInt64(&buf[bufPos], attribs.lastAccessTimeSecs);

   // validAttribs
   bufPos += Serialization::serializeInt(&buf[bufPos], validAttribs);

   // mode
   bufPos += Serialization::serializeInt(&buf[bufPos], attribs.mode);

   // userID
   bufPos += Serialization::serializeUInt(&buf[bufPos], attribs.userID);

   // groupID
   bufPos += Serialization::serializeUInt(&buf[bufPos], attribs.groupID);
   
   // entryID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], entryIDLen, entryID);

   // targetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], targetID);

   // mirroredFromTargetID
   bufPos += Serialization::serializeUShort(&buf[bufPos], mirroredFromTargetID);

   // enableCreation
   bufPos += Serialization::serializeBool(&buf[bufPos], enableCreation);
}

bool SetLocalAttrMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // modificationTimeSecs
   unsigned modificationTimeSecsLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &attribs.modificationTimeSecs,
      &modificationTimeSecsLen) )
      return false;

   bufPos += modificationTimeSecsLen;

   // lastAccessTimeSecs
   unsigned lastAccessTimeSecsLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &attribs.lastAccessTimeSecs,
      &lastAccessTimeSecsLen) )
      return false;

   bufPos += lastAccessTimeSecsLen;

   // validAttribs
   unsigned validAttribsFieldLen;
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &validAttribs,
      &validAttribsFieldLen) )
      return false;

   bufPos += validAttribsFieldLen;

   // mode
   unsigned modeFieldLen;
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &attribs.mode, &modeFieldLen) )
      return false;

   bufPos += modeFieldLen;

   // userID
   unsigned userIDFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &attribs.userID,
      &userIDFieldLen) )
      return false;

   bufPos += userIDFieldLen;

   // groupID
   unsigned groupIDFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &attribs.groupID,
      &groupIDFieldLen) )
      return false;

   bufPos += groupIDFieldLen;

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

   // mirroredFromTargetID

   unsigned mirroredFromTargetIDBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &mirroredFromTargetID, &mirroredFromTargetIDBufLen) )
      return false;

   bufPos += mirroredFromTargetIDBufLen;

   // enableCreation
   unsigned enableCreationBufLen;

   if(!Serialization::deserializeBool(&buf[bufPos], bufLen-bufPos, &enableCreation,
      &enableCreationBufLen) )
      return false;
   
   bufPos += enableCreationBufLen;
   
   return true;
}


