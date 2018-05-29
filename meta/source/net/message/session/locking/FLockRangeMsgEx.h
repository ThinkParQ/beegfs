#ifndef FLOCKRANGEMSGEX_H_
#define FLOCKRANGEMSGEX_H_

#include <common/net/message/session/locking/FLockRangeMsg.h>
#include <common/net/message/session/locking/FLockRangeRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <net/message/MirroredMessage.h>
#include <storage/FileInode.h>


class FLockRangeMsgEx : public MirroredMessage<FLockRangeMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<FLockRangeRespMsg, NETMSGTYPE_FLockRange> ResponseState;

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
         return static_cast<FLockRangeRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "FLockRangeMsgEx/forward"; }
};

#endif /* FLOCKRANGEMSGEX_H_ */
