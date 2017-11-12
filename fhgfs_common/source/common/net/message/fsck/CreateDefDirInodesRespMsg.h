#ifndef CREATEDEFDIRINODESRESPMSG_H
#define CREATEDEFDIRINODESRESPMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/net/message/NetMessage.h>

class CreateDefDirInodesRespMsg : public NetMessageSerdes<CreateDefDirInodesRespMsg>
{
   public:
      CreateDefDirInodesRespMsg(StringList* failedInodeIDs, FsckDirInodeList* createdInodes) :
         BaseType(NETMSGTYPE_CreateDefDirInodesResp)
      {
         this->failedInodeIDs = failedInodeIDs;
         this->createdInodes = createdInodes;
      }

      virtual ~CreateDefDirInodesRespMsg() { };

      CreateDefDirInodesRespMsg() : BaseType(NETMSGTYPE_CreateDefDirInodesResp)
      {
      }

   private:
      StringList* failedInodeIDs;
      FsckDirInodeList* createdInodes;

      struct {
         StringList failedInodeIDs;
         FsckDirInodeList createdInodes;
      } parsed;

   public:
      StringList& getFailedInodeIDs()
      {
         return *failedInodeIDs;
      }

      FsckDirInodeList& getCreatedInodes()
      {
         return *createdInodes;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->failedInodeIDs, obj->parsed.failedInodeIDs)
            % serdes::backedPtr(obj->createdInodes, obj->parsed.createdInodes);
      }
};

#endif /* CREATEDEFDIRINODESRESPMSG_H */
