#include "FindEntrynameMsg.h"

bool FindEntrynameMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // id
   unsigned idBufLen = 0;
   if (!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos, &idLen, &id, &idBufLen))
      return false;
   
   bufPos += idBufLen;

   // dirId
   unsigned dirIdBufLen = 0;
   if (!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos, &dirIdLen, &dirId, &dirIdBufLen))
      return false;

   bufPos += dirIdBufLen;

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

void FindEntrynameMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // id
   bufPos += Serialization::serializeStr(&buf[bufPos], idLen, id);

   // dirId
   bufPos += Serialization::serializeStr(&buf[bufPos], dirIdLen, dirId);

   // currentDirLoc
   bufPos += Serialization::serializeInt64(&buf[bufPos], currentDirLoc);

   // lastEntryLoc
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastEntryLoc);
}

