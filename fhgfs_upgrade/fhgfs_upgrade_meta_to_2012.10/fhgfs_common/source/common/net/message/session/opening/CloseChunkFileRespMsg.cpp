#include "CloseChunkFileRespMsg.h"


bool CloseChunkFileRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // filesize
   unsigned filesizeFieldLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &filesize, &filesizeFieldLen) )
      return false;

   bufPos += filesizeFieldLen;

   // allocedBlocks
   unsigned allocedBlocksFieldLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &allocedBlocks,
      &allocedBlocksFieldLen) )
      return false;

   bufPos += allocedBlocksFieldLen;

   // modificationTimeSecs
   unsigned modificationTimeSecsLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &modificationTimeSecs,
      &modificationTimeSecsLen) )
      return false;

   bufPos += modificationTimeSecsLen;

   // lastAccessTimeSecs
   unsigned lastAccessTimeSecsLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &lastAccessTimeSecs,
      &lastAccessTimeSecsLen) )
      return false;

   bufPos += lastAccessTimeSecsLen;

   // storageVersion
   unsigned versionFieldLen;
   if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos,
      &storageVersion, &versionFieldLen) )
      return false;

   bufPos += versionFieldLen;
   
   // result
   unsigned resultFieldLen;
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &result, &resultFieldLen) )
      return false;

   bufPos += resultFieldLen;

   return true;   
}

void CloseChunkFileRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // filesize
   bufPos += Serialization::serializeInt64(&buf[bufPos], filesize);

   // allocedBlocks
   bufPos += Serialization::serializeInt64(&buf[bufPos], allocedBlocks);

   // modificationTimeSecs
   bufPos += Serialization::serializeInt64(&buf[bufPos], modificationTimeSecs);

   // lastAccessTimeSecs
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastAccessTimeSecs);
   
   // storageVersion
   bufPos += Serialization::serializeUInt64(&buf[bufPos], storageVersion);

   // result
   bufPos += Serialization::serializeInt(&buf[bufPos], result);
}



