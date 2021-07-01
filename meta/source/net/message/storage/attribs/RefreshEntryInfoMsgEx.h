#ifndef REFRESHENTRYINFOMSGEX_H_
#define REFRESHENTRYINFOMSGEX_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoMsg.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <net/message/MirroredMessage.h>

// Update entry info, called by fsck or by fhgfs-ctl

class RefreshEntryInfoMsgEx : public MirroredMessage<RefreshEntryInfoMsg,
   std::tuple<FileIDLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<RefreshEntryInfoRespMsg, NETMSGTYPE_RefreshEntryInfo>
         ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, FileIDLock> lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;
      FhgfsOpsErr refreshInfoRec();
      FhgfsOpsErr refreshInfoRoot();

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<RefreshEntryInfoRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "RefreshEntryInfoMsgEx/forward"; }
};


#endif /* REFRESHENTRYINFOMSGEX_H_ */
