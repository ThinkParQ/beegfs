#pragma once

#include <common/net/message/storage/chunkbalancing/ChunkBalanceMsg.h>
#include <common/net/message/storage/chunkbalancing/ChunkBalanceRespMsg.h>

#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>

class ChunkBalancerJob;

class ChunkBalanceMsgResponseState : public ErrorCodeResponseState<ChunkBalanceRespMsg, NETMSGTYPE_ChunkBalance>
{
   public:

      ChunkBalanceMsgResponseState() : ErrorCodeResponseState<ChunkBalanceRespMsg, NETMSGTYPE_ChunkBalance>(FhgfsOpsErr_INTERNAL)
      {
      }

      ChunkBalanceMsgResponseState(ChunkBalanceMsgResponseState&& other) :
        ErrorCodeResponseState<ChunkBalanceRespMsg, NETMSGTYPE_ChunkBalance>(other.result)
      {
      }

      /*
      // Always return false from changeObservableState() to prohibit forwarding
      // to secondary. See MirroredMessage::finishOperation() for more details.
      */

      bool changesObservableState() const override
      {
         return false;
      }

      void setResult(FhgfsOpsErr result) { this->result = result; }

   private:
      FhgfsOpsErr result;
};

class ChunkBalanceMsgEx : public MirroredMessage<ChunkBalanceMsg, FileIDLock>
{
   public:
      typedef ChunkBalanceMsgResponseState ResponseState;  
      
      virtual bool processIncoming(ResponseContext& ctx) override;
 
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;
   
      FileIDLock lock(EntryLockStore& store) override;
      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private: 
      Mutex ChunkBalanceJobMutex;
      ChunkBalancerJob* addChunkBalanceJob();

      void forwardToSecondary(ResponseContext& ctx) override {}; 
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return FhgfsOpsErr_SUCCESS;    
      }
      const char* mirrorLogContext() const override { return "ChunkBalanceMsgEx/forward"; } 

};

