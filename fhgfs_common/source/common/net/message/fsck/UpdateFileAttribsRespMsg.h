#ifndef UPDATEFILEATTRIBSRESPMSG_H
#define UPDATEFILEATTRIBSRESPMSG_H

#include <common/fsck/FsckFileInode.h>
#include <common/net/message/NetMessage.h>

class UpdateFileAttribsRespMsg : public NetMessageSerdes<UpdateFileAttribsRespMsg>
{
   public:
      UpdateFileAttribsRespMsg(FsckFileInodeList* failedInodes) :
         BaseType(NETMSGTYPE_UpdateFileAttribsResp)
      {
         this->failedInodes = failedInodes;
      }

      UpdateFileAttribsRespMsg() : BaseType(NETMSGTYPE_UpdateFileAttribsResp)
      {
      }

   private:
      FsckFileInodeList* failedInodes;

      struct {
         FsckFileInodeList failedInodes;
      } parsed;

   public:
      FsckFileInodeList& getFailedInodes()
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


#endif /*UPDATEFILEATTRIBSRESPMSG_H*/
