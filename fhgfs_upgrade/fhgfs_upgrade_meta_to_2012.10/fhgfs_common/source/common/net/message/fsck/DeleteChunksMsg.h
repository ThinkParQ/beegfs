#ifndef DELETECHUNKSMSG_H
#define DELETECHUNKSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class DeleteChunksMsg : public NetMessage
{
   public:
      DeleteChunksMsg(FsckChunkList* chunks) : NetMessage(NETMSGTYPE_DeleteChunks)
      {
         this->chunks = chunks;
      }


   protected:
      DeleteChunksMsg() : NetMessage(NETMSGTYPE_DeleteChunks)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckChunkList(this->chunks); // chunks
      }


   private:
      FsckChunkList* chunks;
      unsigned chunksBufLen;
      unsigned chunksElemNum;
      const char* chunksStart;

   public:
      void parseChunks(FsckChunkList* outChunks)
      {
         SerializationFsck::deserializeFsckChunkList(chunksElemNum, chunksStart, outChunks);
      }
};


#endif /*DELETECHUNKSMSG_H*/
