#pragma once

#include <common/net/message/storage/chunkbalancing/StripePatternUpdateMsg.h>
#include <common/net/message/storage/chunkbalancing/StripePatternUpdateRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>


class ChunkBalancerJob;

class StripePatternUpdateMsgEx : public MirroredMessage<StripePatternUpdateMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<StripePatternUpdateRespMsg, NETMSGTYPE_StripePatternUpdate> ResponseState;
      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FileIDLock lock(EntryLockStore& store) override;
      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private: 
      Mutex ChunkBalanceJobMutex;
      Mutex StripePatternUpdateMutex;
      Condition StripePatternUpdateCond;

      ChunkBalancerJob* addChunkBalanceJob();
      void waitForStripePatternUpdate();

      void forwardToSecondary(ResponseContext& ctx) override;
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return static_cast<FhgfsOpsErr>(static_cast<StripePatternUpdateRespMsg&>(resp).getValue());
      }
      const char* mirrorLogContext() const override { return "StripePatternUpdateMsgEx/forward"; }
};

