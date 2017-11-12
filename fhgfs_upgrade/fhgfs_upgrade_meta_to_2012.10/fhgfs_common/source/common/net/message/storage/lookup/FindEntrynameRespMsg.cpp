#include "FindEntrynameRespMsg.h"

bool FindEntrynameRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // entriesLeft
   unsigned entriesLeftLen;
   if (!Serialization::deserializeBool(&buf[bufPos],bufLen-bufPos,&entriesLeft,&entriesLeftLen))
      return false;

   bufPos += entriesLeftLen;

   // entryName
   unsigned entryNameBufLen = 0;
   if (!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos, &entryNameLen, &entryName,
      &entryNameBufLen))
      return false;
   
   bufPos += entryNameBufLen;

   // parentID
   unsigned parentIDBufLen = 0;
   if (!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos, &parentIDLen, &parentID,
      &parentIDBufLen))
      return false;

   bufPos += parentIDBufLen;

   // currentDirLoc

   uint currentDirLocLen;

   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
      &currentDirLoc, &currentDirLocLen))
      return false;

   bufPos += currentDirLocLen;

   // lastEntryLoc

   uint lastEntryLocLen;

   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
      &lastEntryLoc, &lastEntryLocLen))
      return false;

   bufPos += lastEntryLocLen; // not needed right now. included to avoid human errors later ;)
   
   return true;
}

void FindEntrynameRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // entriesLeft
   bufPos += Serialization::serializeBool(&buf[bufPos], entriesLeft);

   // entryName
   bufPos += Serialization::serializeStr(&buf[bufPos], entryNameLen, entryName);

   // parentID
   bufPos += Serialization::serializeStr(&buf[bufPos], parentIDLen, parentID);

   // currentDirLoc
   bufPos += Serialization::serializeInt64(&buf[bufPos], currentDirLoc);

   // lastEntryLoc
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastEntryLoc);
}

