#include "SetLastBuddyCommOverrideMsgEx.h"

#include <common/net/message/storage/mirroring/SetLastBuddyCommOverrideRespMsg.h>
#include <program/Program.h>

bool SetLastBuddyCommOverrideMsgEx::processIncoming(ResponseContext& ctx)
{
   uint16_t targetID = getTargetID();
   int64_t timestamp = getTimestamp();
   bool abortResync = getAbortResync();

   App* app = Program::getApp();
   StorageTargets* storageTargets = app->getStorageTargets();

   const auto target = storageTargets->getTarget(targetID);
   if (!target)
   {
      ctx.sendResponse(SetLastBuddyCommOverrideRespMsg(FhgfsOpsErr_UNKNOWNTARGET));
      return true;
   }

   target->setLastBuddyComm(std::chrono::system_clock::from_time_t(timestamp), true);

   if (abortResync)
   {
      BuddyResyncJob* resyncJob = app->getBuddyResyncer()->getResyncJob(targetID);
      if (resyncJob)
         resyncJob->abort();
   }

   ctx.sendResponse(SetLastBuddyCommOverrideRespMsg(FhgfsOpsErr_SUCCESS));

   return true;
}
