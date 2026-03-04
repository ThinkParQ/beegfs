#pragma once

#include <common/net/message/storage/chunkbalancing/UpdateStripePatternMsg.h>
#include <common/net/message/storage/chunkbalancing/UpdateStripePatternRespMsg.h>
#include <components/chunkbalancer/ChunkBalancerJob.h>
#include <net/message/MirroredMessage.h>
#include <app/App.h>
#include <session/EntryLock.h>

class  UpdateStripePatternMsgEx : public MirroredMessage<UpdateStripePatternMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<UpdateStripePatternRespMsg, NETMSGTYPE_UpdateStripePattern> ResponseState;
      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FileIDLock lock(EntryLockStore& store) override;
      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private: 
      bool setStripePattern(EntryInfo* entryInfo, FileInode& inode, std::string& relativePath, uint16_t localTargetID, uint16_t destinationID);
      bool checkChunkOnStorageTarget(FileInode& inode, std::string& relativePath, uint16_t targetID);
      void forwardToSecondary(ResponseContext& ctx) override;
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return static_cast<FhgfsOpsErr>(static_cast<UpdateStripePatternRespMsg&>(resp).getValue());
      }
      const char* mirrorLogContext() const override { return "UpdateStripePatternMsgEx/forward"; }
};

