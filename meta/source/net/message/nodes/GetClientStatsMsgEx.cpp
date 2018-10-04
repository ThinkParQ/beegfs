#include <program/Program.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/nodes/OpCounter.h>
#include "GetClientStatsMsgEx.h"
#include <nodes/MetaNodeOpStats.h>
#include <common/net/message/nodes/GetClientStatsRespMsg.h>


/**
 * Server side gets a GetClientStatsMsgEx request
 */
bool GetClientStatsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetClientStatsMsgEx incoming");

   uint64_t cookieIP = getCookieIP(); // requested is cookie+1

   // get stats
   MetaNodeOpStats* opStats = Program::getApp()->getNodeOpStats();

   bool wantPerUserStats = isMsgHeaderFeatureFlagSet(GETCLIENTSTATSMSG_FLAG_PERUSERSTATS);
   UInt64Vector opStatsVec;

   opStats->mapToUInt64Vec(
      cookieIP, GETCLIENTSTATSRESP_MAX_PAYLOAD_LEN, wantPerUserStats, &opStatsVec);

   ctx.sendResponse(GetClientStatsRespMsg(&opStatsVec) );

   return true;
}

