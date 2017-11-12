#ifndef FETCHFSCKCHUNKLISTRESPMSG_H
#define FETCHFSCKCHUNKLISTRESPMSG_H

#include <common/fsck/FsckChunk.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/FsckTk.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/serialization/Serialization.h>

class FetchFsckChunkListRespMsg: public NetMessageSerdes<FetchFsckChunkListRespMsg>
{
   public:
      FetchFsckChunkListRespMsg(FsckChunkList* chunkList, FetchFsckChunkListStatus status) :
         BaseType(NETMSGTYPE_FetchFsckChunkListResp)
      {
         this->chunkList = chunkList;
         this->status = status;
      }

      // only for deserialization
      FetchFsckChunkListRespMsg() : BaseType(NETMSGTYPE_FetchFsckChunkListResp)
      {
      }

   private:
      FsckChunkList* chunkList;
      FetchFsckChunkListStatus status;

      // for deserialization
      struct {
         FsckChunkList chunkList;
      } parsed;

   public:
      // getter
      FetchFsckChunkListStatus getStatus()
      {
         return this->status;
      }

      FsckChunkList& getChunkList()
      {
         return *chunkList;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->chunkList, obj->parsed.chunkList)
            % serdes::as<uint32_t>(obj->status);
      }
};

#endif /*FETCHFSCKCHUNKLISTRESPMSG_H*/
