#ifndef DELETECHUNKSMSG_H
#define DELETECHUNKSMSG_H

#include <common/fsck/FsckChunk.h>
#include <common/net/message/NetMessage.h>

class DeleteChunksMsg : public NetMessageSerdes<DeleteChunksMsg>
{
   public:
      DeleteChunksMsg(FsckChunkList* chunks) : BaseType(NETMSGTYPE_DeleteChunks)
      {
         this->chunks = chunks;
      }

      DeleteChunksMsg() : BaseType(NETMSGTYPE_DeleteChunks)
      {
      }

   private:
      FsckChunkList* chunks;

      struct {
         FsckChunkList chunks;
      } parsed;

   public:
      FsckChunkList& getChunks()
      {
         return *chunks;
      }

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         DeleteChunksMsg* other = (DeleteChunksMsg*) msg;
         return *chunks == *other->chunks
            ? TestingEqualsRes_TRUE
            : TestingEqualsRes_FALSE;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->chunks, obj->parsed.chunks);
      }
};


#endif /*DELETECHUNKSMSG_H*/
