#include <toolkit/StorageTkEx.h>
#include <program/Program.h>

#include "StripePatternUpdateMsgEx.h"

FileIDLock StripePatternUpdateMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

bool StripePatternUpdateMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
   const char* logContext = "StripePatternUpdateMsg incoming";
   #endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_SPAM, "Attempting to change stripe pattern of file at chunkPath: " + getRelativePath() + "; from localTargetID: "
         + StringTk::uintToStr(getTargetID()) + "; to destinationTargetID: "
         + StringTk::uintToStr(getDestinationID()));
   return BaseType::processIncoming(ctx);
   
}

std::unique_ptr<MirroredMessageResponseState> StripePatternUpdateMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{

   FhgfsOpsErr stripePatternMsgRes;
   const char* logContext = "Update Stripe Pattern";

   LogContext(logContext).logErr("This message is not yet implemented. \n It should change the metadata stripe pattern of the chunk that is being balanced. ");

   stripePatternMsgRes = FhgfsOpsErr_SUCCESS;
   return boost::make_unique<ResponseState>(stripePatternMsgRes);
}

ChunkBalancerJob*  StripePatternUpdateMsgEx::addChunkBalanceJob()
{
   std::lock_guard<Mutex> mutexLock(ChunkBalanceJobMutex);
      
   ChunkBalancerJob* chunkBalanceJob = nullptr;
   return chunkBalanceJob; 
}

void  StripePatternUpdateMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_StripePatternUpdate);
}