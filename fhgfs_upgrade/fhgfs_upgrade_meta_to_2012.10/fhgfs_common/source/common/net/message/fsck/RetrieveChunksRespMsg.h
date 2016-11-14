#ifndef RETRIEVECHUNKSREPMSG_H
#define RETRIEVECHUNKSREPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RetrieveChunksRespMsg : public NetMessage
{
   public:
      RetrieveChunksRespMsg (FsckChunkList* chunks, int64_t lastOffset):
         NetMessage(NETMSGTYPE_RetrieveChunksResp)
      {
         this->chunks = chunks;
         this->lastOffset = lastOffset;
      }

      RetrieveChunksRespMsg() : NetMessage(NETMSGTYPE_RetrieveChunksResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckChunkList(this->chunks) + // chunks
            Serialization::serialLenInt64(); // lastOffset
      }


   private:
      FsckChunkList* chunks;
      int64_t lastOffset;

      // for deserialization
      unsigned chunksElemNum;
      const char* chunksStart;
      unsigned chunksBufLen;

   public:
      // inliners
      void parseChunkList(FsckChunkList* outChunks)
      {
         SerializationFsck::deserializeFsckChunkList(chunksElemNum, chunksStart, outChunks);
      }

      // getters & setters
      int64_t getLastOffset()
      {
         return this->lastOffset;
      }
};


#endif /*RETRIEVECHUNKSREPMSG_H*/
