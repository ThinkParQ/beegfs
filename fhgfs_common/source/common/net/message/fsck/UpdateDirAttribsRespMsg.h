#ifndef UPDATEDIRATTRIBSRESPMSG_H
#define UPDATEDIRATTRIBSRESPMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/net/message/NetMessage.h>

class UpdateDirAttribsRespMsg : public NetMessageSerdes<UpdateDirAttribsRespMsg>
{
   public:
      UpdateDirAttribsRespMsg(FsckDirInodeList* failedInodes) :
         BaseType(NETMSGTYPE_UpdateDirAttribsResp)
      {
         this->failedInodes = failedInodes;
      }

      UpdateDirAttribsRespMsg() : BaseType(NETMSGTYPE_UpdateDirAttribsResp)
      {
      }

   private:
      FsckDirInodeList* failedInodes;

      struct {
         FsckDirInodeList failedInodes;
      } parsed;

   public:
      FsckDirInodeList& getFailedInodes()
      {
         return *failedInodes;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->failedInodes, obj->parsed.failedInodes);
      }
};


#endif /*UPDATEDIRATTRIBSRESPMSG_H*/
