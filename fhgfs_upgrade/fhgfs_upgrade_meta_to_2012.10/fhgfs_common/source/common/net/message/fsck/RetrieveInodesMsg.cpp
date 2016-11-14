#include "RetrieveInodesMsg.h"

bool RetrieveInodesMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   uint hashDirNumBufLen;

   if (!Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &hashDirNum,
      &hashDirNumBufLen))
      return false;

   bufPos += hashDirNumBufLen;
   
   //currentDirLoc
   uint lastOffsetBufLen;

   if (!Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &lastOffset,
      &lastOffsetBufLen))
      return false;

   bufPos += lastOffsetBufLen;

   uint maxOutInodesBufLen;

   if (!Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &maxOutInodes,
      &maxOutInodesBufLen))
      return false;

   bufPos += maxOutInodesBufLen;

   return true;
}

void RetrieveInodesMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;


   bufPos += Serialization::serializeUInt(&buf[bufPos], hashDirNum);

   bufPos += Serialization::serializeInt64(&buf[bufPos], lastOffset);

   bufPos += Serialization::serializeUInt(&buf[bufPos], maxOutInodes);
}

