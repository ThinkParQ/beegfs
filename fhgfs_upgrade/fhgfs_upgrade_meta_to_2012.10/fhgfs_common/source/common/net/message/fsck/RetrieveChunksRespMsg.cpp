#include "RetrieveChunksRespMsg.h"

bool RetrieveChunksRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   {
      // chunks
      if ( !SerializationFsck::deserializeFsckChunkListPreprocess(&buf[bufPos], bufLen - bufPos,
         &chunksStart, &chunksElemNum, &chunksBufLen) )
         return false;

      bufPos += chunksBufLen;
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

void RetrieveChunksRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // chunks
   bufPos += SerializationFsck::serializeFsckChunkList(&buf[bufPos], chunks);

   // lastOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastOffset);
}

