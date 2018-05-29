#ifndef FLOCKENTRYMSGEX_H_
#define FLOCKENTRYMSGEX_H_

#include <common/net/message/session/locking/FLockEntryMsg.h>
#include <common/net/message/session/locking/FLockEntryRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <net/message/MirroredMessage.h>
#include <storage/FileInode.h>


class FLockEntryMsgEx : public MirroredMessage<FLockEntryMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<FLockEntryRespMsg, NETMSGTYPE_FLockEntry> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      FhgfsOpsErr clientResult;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return static_cast<FLockEntryRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "FLockEntryMsgEx/forward"; }
};

#endif /* FLOCKENTRYMSGEX_H_ */
