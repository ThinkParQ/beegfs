#ifndef HARDLINKEX_H_
#define HARDLINKEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/HardlinkMsg.h>
#include <common/net/message/storage/creating/HardlinkRespMsg.h>
#include <session/EntryLock.h>
#include <storage/DirEntry.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>

class HardlinkMsgEx : public MirroredMessage<HardlinkMsg,
   std::tuple<FileIDLock, ParentNameLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<HardlinkRespMsg, NETMSGTYPE_Hardlink> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, ParentNameLock, ParentNameLock, FileIDLock>
         lock(EntryLockStore& store) override;

      bool isMirrored() override { return getFromInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<HardlinkRespMsg&>(resp).getValue();
      }

      FhgfsOpsErr incDecRemoteLinkCount(NumNodeID const& ownerNodeID, bool increment);
      const char* mirrorLogContext() const override { return "HardlinkMsgEx/forward"; }
};


#endif /*HARDLINKEX_H_*/
