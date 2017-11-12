#ifndef RECREATEFSIDSRESPMSG_H
#define RECREATEFSIDSRESPMSG_H

#include <common/fsck/FsckDirEntry.h>
#include <common/net/message/NetMessage.h>

class RecreateFsIDsRespMsg : public NetMessageSerdes<RecreateFsIDsRespMsg>
{
   public:
      RecreateFsIDsRespMsg(FsckDirEntryList* failedEntries) :
         BaseType(NETMSGTYPE_RecreateFsIDsResp)
      {
         this->failedEntries = failedEntries;
      }

      RecreateFsIDsRespMsg() : BaseType(NETMSGTYPE_RecreateFsIDsResp)
      {
      }

   private:
      FsckDirEntryList* failedEntries;

      struct {
         FsckDirEntryList failedEntries;
      } parsed;

   public:
      FsckDirEntryList& getFailedEntries()
      {
         return *failedEntries;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->failedEntries, obj->parsed.failedEntries);
      }
};


#endif /*RECREATEFSIDSRESPMSG_H*/
