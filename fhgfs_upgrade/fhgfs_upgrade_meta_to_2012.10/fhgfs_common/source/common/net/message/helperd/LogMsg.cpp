#include "LogMsg.h"

bool LogMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // level
   
   unsigned levelBufLen;
   
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &level, &levelBufLen) )
      return false;
   
   bufPos += levelBufLen;

   // threadID

   unsigned threadIDBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &threadID, &threadIDBufLen) )
      return false;

   bufPos += threadIDBufLen;

   // threadName
   
   unsigned threadNameBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &threadNameLen, &threadName, &threadNameBufLen) )
      return false;
   
   bufPos += threadNameBufLen;

   // context
   
   unsigned contextBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &contextLen, &context, &contextBufLen) )
      return false;
   
   bufPos += contextBufLen;

   // logMsg
   
   unsigned logMsgBufLen;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &logMsgLen, &logMsg, &logMsgBufLen) )
      return false;
   
   bufPos += logMsgBufLen;

   return true;   
}

void LogMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // level
   bufPos += Serialization::serializeInt(&buf[bufPos], level);

   // threadID
   bufPos += Serialization::serializeInt(&buf[bufPos], threadID);

   // threadName
   bufPos += Serialization::serializeStr(&buf[bufPos], threadNameLen, threadName);

   // context
   bufPos += Serialization::serializeStr(&buf[bufPos], contextLen, context);

   // logMsg
   bufPos += Serialization::serializeStr(&buf[bufPos], logMsgLen, logMsg);
}



