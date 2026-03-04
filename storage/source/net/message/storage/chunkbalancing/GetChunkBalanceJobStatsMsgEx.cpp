#include "GetChunkBalanceJobStatsMsgEx.h"

#include <common/net/message/storage/chunkbalancing/GetChunkBalanceJobStatsRespMsg.h>
#include <program/Program.h>

bool GetChunkBalanceJobStatsMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   ChunkBalancerJob* chunkBalancerJob = app->getChunkBalancerJob();
   ChunkBalancerJobStatistics jobStats;

   if (chunkBalancerJob)
      chunkBalancerJob->getJobStats(&jobStats);

   ctx.sendResponse(GetChunkBalanceJobStatsRespMsg(&jobStats) );

   return true;
}
