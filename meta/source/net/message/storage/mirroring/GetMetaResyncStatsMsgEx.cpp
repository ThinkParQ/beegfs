#include "GetMetaResyncStatsMsgEx.h"

#include <common/net/message/storage/mirroring/GetMetaResyncStatsRespMsg.h>
#include <components/buddyresyncer/BuddyResyncer.h>
#include <program/Program.h>

bool GetMetaResyncStatsMsgEx::processIncoming(ResponseContext& ctx)
{
   BuddyResyncer* resyncer = Program::getApp()->getBuddyResyncer();

   MetaBuddyResyncJobStatistics stats;

   BuddyResyncJob* job = resyncer->getResyncJob();
   if (job)
      stats = job->getJobStats();

   ctx.sendResponse(GetMetaResyncStatsRespMsg(&stats));

   return true;
}
