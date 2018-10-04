#include <program/Program.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/nodes/OpCounter.h>
#include "GetClientStatsMsgEx.h"
#include <nodes/StorageNodeOpStats.h>
#include <common/net/message/nodes/GetClientStatsRespMsg.h>


/**
 * Server side, called when the server gets a GetClientStatsMsgEx request
 */
bool GetClientStatsMsgEx::processIncoming(ResponseContext& ctx)
{
   uint64_t cookieIP = getCookieIP(); // requested is cookie+1

   // get stats
   StorageNodeOpStats* clientOpStats = Program::getApp()->getNodeOpStats();

   bool wantPerUserStats = isMsgHeaderFeatureFlagSet(GETCLIENTSTATSMSG_FLAG_PERUSERSTATS);
   UInt64Vector opStatsVec;

   clientOpStats->mapToUInt64Vec(
      cookieIP, GETCLIENTSTATSRESP_MAX_PAYLOAD_LEN, wantPerUserStats, &opStatsVec);

   ctx.sendResponse(GetClientStatsRespMsg(&opStatsVec) );

   return true;
}

