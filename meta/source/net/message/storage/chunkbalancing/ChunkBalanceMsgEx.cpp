#include <toolkit/StorageTkEx.h>
#include <program/Program.h>

#include "ChunkBalanceMsgEx.h"


FileIDLock ChunkBalanceMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

bool ChunkBalanceMsgEx::processIncoming(ResponseContext& ctx)
{ 
   #ifdef BEEGFS_DEBUG
   const char* logContext = "ChunkBalanceMsg incoming";
   #endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_SPAM, "Starting ChunkBalance job from localTargetID: "
         + StringTk::uintToStr(getTargetID()) + "; to destinationTargetID: "
         + StringTk::uintToStr(getDestinationID()));
   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> ChunkBalanceMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   ChunkBalanceMsgResponseState resp;

   FhgfsOpsErr chunkBalanceMsgRes = FhgfsOpsErr_SUCCESS;
 
   const char* logContext = "Update Stripe Pattern";

   LogContext(logContext).logErr("This message is not yet implemented. \n It should trigger chunk balancing components on the metadata service. ");

   resp.setResult(chunkBalanceMsgRes);
   return boost::make_unique<ResponseState>(std::move(resp));
}

ChunkBalancerJob*  ChunkBalanceMsgEx::addChunkBalanceJob()
{
   std::lock_guard<Mutex> mutexLock(ChunkBalanceJobMutex);
      
   ChunkBalancerJob* chunkBalanceJob = nullptr;
   return chunkBalanceJob; 
}
