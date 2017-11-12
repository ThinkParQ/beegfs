#include "RetrieveInodesRespMsg.h"

bool RetrieveInodesRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // FsckFileInodeList
   if(!SerializationFsck::deserializeFsckFileInodeListPreprocess(&buf[bufPos], bufLen-bufPos,
      &fsckFileInodeListStart, &fsckFileInodeListElemNum, &fsckFileInodeListBufLen) )
      return false;
   
   bufPos += fsckFileInodeListBufLen;

   // FsckDirInodeList
   if(!SerializationFsck::deserializeFsckDirInodeListPreprocess(&buf[bufPos], bufLen-bufPos,
      &fsckDirInodeListStart, &fsckDirInodeListElemNum, &fsckDirInodeListBufLen) )
      return false;

   bufPos += fsckDirInodeListBufLen;

   // lastOffset
   unsigned lastOffsetBufLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &lastOffset, &lastOffsetBufLen))
      return false;

   bufPos += lastOffsetBufLen;

   return true;
}

void RetrieveInodesRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // FsckFileInodeList
   bufPos += SerializationFsck::serializeFsckFileInodeList(&buf[bufPos], fileInodes);
   
   // FsckDirInodeList
   bufPos += SerializationFsck::serializeFsckDirInodeList(&buf[bufPos], dirInodes);

   // lastOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastOffset);
}

