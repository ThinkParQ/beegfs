#ifndef FIXINODEOWNERSINDENTRYRESPMSG_H
#define FIXINODEOWNERSINDENTRYRESPMSG_H

#include <common/fsck/FsckDirEntry.h>
#include <common/net/message/NetMessage.h>

class FixInodeOwnersInDentryRespMsg : public NetMessageSerdes<FixInodeOwnersInDentryRespMsg>
{
   public:
      FixInodeOwnersInDentryRespMsg(FsckDirEntryList* failedEntries) :
         BaseType(NETMSGTYPE_FixInodeOwnersInDentryResp)
      {
         this->failedEntries = failedEntries;
      }

      FixInodeOwnersInDentryRespMsg() : BaseType(NETMSGTYPE_FixInodeOwnersInDentryResp)
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


#endif /*FIXINODEOWNERSINDENTRYRESPMSG_H*/
