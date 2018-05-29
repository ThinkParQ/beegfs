#include "GetMetaResyncStatsMsgEx.h"

#include <common/net/message/storage/mirroring/GetMetaResyncStatsRespMsg.h>
#include <components/buddyresyncer/BuddyResyncer.h>
#include <program/Program.h>

bool GetMetaResyncStatsMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "GetMetaResyncStatsMsg incoming";

   LOG_DEBUG(logContext, Log_DEBUG,
      "Received a GetMetasResyncStatsMsg from: " + ctx.peerName() );
#endif // BEEGFS_DEBUG

   BuddyResyncer* resyncer = Program::getApp()->getBuddyResyncer();

   MetaBuddyResyncJobStatistics stats;

   BuddyResyncJob* job = resyncer->getResyncJob();
   if (job)
      stats = job->getJobStats();

   ctx.sendResponse(GetMetaResyncStatsRespMsg(&stats));

   return true;
}
