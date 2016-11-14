#ifndef DELETECHUNKSRESPMSG_H
#define DELETECHUNKSRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class DeleteChunksRespMsg : public NetMessage
{
   public:
      DeleteChunksRespMsg(FsckChunkList* failedChunks) : NetMessage(NETMSGTYPE_DeleteChunksResp)
      {
         this->failedChunks = failedChunks;
      }

      DeleteChunksRespMsg() : NetMessage(NETMSGTYPE_DeleteChunksResp)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckChunkList(this->failedChunks); // failedChunks
      }


   private:
      FsckChunkList* failedChunks;
      unsigned chunksBufLen;
      unsigned chunksElemNum;
      const char* chunksStart;

   public:
      void parseFailedChunks(FsckChunkList* outChunks)
      {
         SerializationFsck::deserializeFsckChunkList(chunksElemNum, chunksStart, outChunks);
      }
};


#endif /*DELETECHUNKSRESPMSG_H*/
