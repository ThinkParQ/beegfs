#include "GetStorageResyncStatsMsgEx.h"

#include <common/net/message/storage/mirroring/GetStorageResyncStatsRespMsg.h>
#include <program/Program.h>

bool GetStorageResyncStatsMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   BuddyResyncer* buddyResyncer = app->getBuddyResyncer();
   uint16_t targetID = getTargetID();
   StorageBuddyResyncJobStatistics jobStats;

   BuddyResyncJob* resyncJob = buddyResyncer->getResyncJob(targetID);

   if (resyncJob)
      resyncJob->getJobStats(jobStats);

   ctx.sendResponse(GetStorageResyncStatsRespMsg(&jobStats) );

   return true;
}
