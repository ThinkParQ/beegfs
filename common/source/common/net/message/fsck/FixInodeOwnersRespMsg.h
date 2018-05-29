#ifndef FIXINODEOWNERSRESPMSG_H
#define FIXINODEOWNERSRESPMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/net/message/NetMessage.h>

class FixInodeOwnersRespMsg : public NetMessageSerdes<FixInodeOwnersRespMsg>
{
   public:
      FixInodeOwnersRespMsg(FsckDirInodeList* failedInodes) :
         BaseType(NETMSGTYPE_FixInodeOwnersResp)
      {
         this->failedInodes = failedInodes;
      }

      FixInodeOwnersRespMsg() : BaseType(NETMSGTYPE_FixInodeOwnersResp)
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
         ctx % serdes::backedPtr(obj->failedInodes, obj->parsed.failedInodes);
      }
};


#endif /*FIXINODEOWNERSRESPMSG_H*/
