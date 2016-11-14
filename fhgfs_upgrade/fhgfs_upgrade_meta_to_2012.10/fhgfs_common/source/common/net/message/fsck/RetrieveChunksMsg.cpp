#include "RetrieveChunksMsg.h"

bool RetrieveChunksMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   {
      // targetID
      unsigned targetIDBufLen;

      if ( !Serialization::deserializeUShort(&buf[bufPos], bufLen - bufPos, &targetID,
         &targetIDBufLen) )
         return false;

      bufPos += targetIDBufLen;
   }

   {
      // hashDirNum
      unsigned hashDirNumBufLen;

      if ( !Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &hashDirNum,
         &hashDirNumBufLen) )
         return false;

      bufPos += hashDirNumBufLen;
   }

   {
      // maxOutChunks
      unsigned maxOutChunksBufLen;

      if ( !Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &maxOutChunks,
         &maxOutChunksBufLen) )
         return false;

      bufPos += maxOutChunksBufLen;
   }

   {
      // lastOffset
      unsigned lastOffsetBufLen;

      if ( !Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &lastOffset,
         &lastOffsetBufLen) )
         return false;

      bufPos += lastOffsetBufLen;
   }
   
   return true;
}

void RetrieveChunksMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   bufPos += Serialization::serializeUShort(&buf[bufPos], targetID);

   // hashDirNum
   bufPos += Serialization::serializeUInt(&buf[bufPos], hashDirNum);

   // maxOutChunks
   bufPos += Serialization::serializeUInt(&buf[bufPos], maxOutChunks);

   // lastOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastOffset);
}

