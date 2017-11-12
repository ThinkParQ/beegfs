#ifndef DELETECHUNKSRESPMSG_H
#define DELETECHUNKSRESPMSG_H

#include <common/fsck/FsckChunk.h>
#include <common/net/message/NetMessage.h>

class DeleteChunksRespMsg : public NetMessageSerdes<DeleteChunksRespMsg>
{
   public:
      DeleteChunksRespMsg(FsckChunkList* failedChunks)
         : BaseType(NETMSGTYPE_DeleteChunksResp)
      {
         this->failedChunks = failedChunks;
      }

      DeleteChunksRespMsg() : BaseType(NETMSGTYPE_DeleteChunksResp)
      {
      }

   private:
      FsckChunkList* failedChunks;

      struct {
         FsckChunkList failedChunks;
      } parsed;

   public:
      FsckChunkList& getFailedChunks()
      {
         return *failedChunks;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->failedChunks, obj->parsed.failedChunks);
      }
};


#endif /*DELETECHUNKSRESPMSG_H*/
