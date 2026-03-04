#pragma once

#include <common/net/message/storage/chunkbalancing/StartChunkBalanceMsg.h>
#include <common/net/message/storage/chunkbalancing/StartChunkBalanceRespMsg.h>
#include <components/chunkbalancer/ChunkBalancerJob.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>

class StartChunkBalanceMsgResponseState : public ErrorCodeResponseState<StartChunkBalanceRespMsg, NETMSGTYPE_StartChunkBalance>
{
   public:

      StartChunkBalanceMsgResponseState() : ErrorCodeResponseState<StartChunkBalanceRespMsg, NETMSGTYPE_StartChunkBalance>(FhgfsOpsErr_INTERNAL)
      {
      }

      StartChunkBalanceMsgResponseState(StartChunkBalanceMsgResponseState&& other) :
        ErrorCodeResponseState<StartChunkBalanceRespMsg, NETMSGTYPE_StartChunkBalance>(other.result)
      {
      }

      /*
      // Always return false from changesObservableState() to prohibit forwarding
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

class StartChunkBalanceMsgEx : public MirroredMessage<StartChunkBalanceMsg, FileIDLock>
{
   public:
      typedef StartChunkBalanceMsgResponseState ResponseState;  
      
      virtual bool processIncoming(ResponseContext& ctx) override;
 
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;
   
      FileIDLock lock(EntryLockStore& store) override;
      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }


   private: 
      ChunkBalancerJob* addChunkBalanceJob(bool& outIsNew);

      void forwardToSecondary(ResponseContext& ctx) override {}; 
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return FhgfsOpsErr_SUCCESS;    
      }
      const char* mirrorLogContext() const override { return "StartChunkBalanceMsgEx/forward"; } 

};
