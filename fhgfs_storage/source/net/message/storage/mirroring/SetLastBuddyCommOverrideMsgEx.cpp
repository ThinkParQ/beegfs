#include "SetLastBuddyCommOverrideMsgEx.h"

#include <common/net/message/storage/mirroring/SetLastBuddyCommOverrideRespMsg.h>
#include <program/Program.h>

bool SetLastBuddyCommOverrideMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "SetLastBuddyCommOverrideMsg incoming";

      LOG_DEBUG(logContext, Log_DEBUG,
         "Received a SetLastBuddyCommOverrideMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   FhgfsOpsErr result;
   uint16_t targetID = getTargetID();
   int64_t timestamp = getTimestamp();
   bool abortResync = getAbortResync();

   App* app = Program::getApp();
   StorageTargets* storageTargets = app->getStorageTargets();

   result = storageTargets->overrideLastBuddyComm(targetID, timestamp);

   if (abortResync)
   {
      BuddyResyncJob* resyncJob = app->getBuddyResyncer()->getResyncJob(targetID);
      if (resyncJob)
         resyncJob->abort();
   }

   ctx.sendResponse(SetLastBuddyCommOverrideRespMsg(result) );

   return true;
}
