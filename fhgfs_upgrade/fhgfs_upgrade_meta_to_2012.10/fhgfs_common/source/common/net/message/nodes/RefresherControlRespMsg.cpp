#include "RefresherControlRespMsg.h"


bool RefresherControlRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // isRunning

   unsigned isRunningBufLen;

   if(!Serialization::deserializeBool(&buf[bufPos], bufLen-bufPos,
      &isRunning, &isRunningBufLen) )
      return false;

   bufPos += isRunningBufLen;

   // numDirsRefreshed

   unsigned numDirsRefreshedBufLen;

   if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos,
      &numDirsRefreshed, &numDirsRefreshedBufLen) )
      return false;

   bufPos += numDirsRefreshedBufLen;

   // numFilesRefreshed

   unsigned numFilesRefreshedBufLen;

   if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos,
      &numFilesRefreshed, &numFilesRefreshedBufLen) )
      return false;

   bufPos += numFilesRefreshedBufLen;

   return true;
}

void RefresherControlRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // isRunning
   bufPos += Serialization::serializeBool(&buf[bufPos], isRunning);

   // numDirsRefeshed
   bufPos += Serialization::serializeUInt64(&buf[bufPos], numDirsRefreshed);

   // numFilesRefreshed
   bufPos += Serialization::serializeUInt64(&buf[bufPos], numFilesRefreshed);
}




