#include "DeleteChunksRespMsg.h"

bool DeleteChunksRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // chunks
   if (!SerializationFsck::deserializeFsckChunkListPreprocess(&buf[bufPos],bufLen-bufPos,
      &chunksStart, &chunksElemNum, &chunksBufLen))
      return false;

   bufPos += chunksBufLen;

   return true;
}

void DeleteChunksRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // entryIDs
   bufPos += SerializationFsck::serializeFsckChunkList(&buf[bufPos], failedChunks);
}
