#include <program/Program.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include "GetHighResStatsMsgEx.h"


bool GetHighResStatsMsgEx::processIncoming(ResponseContext& ctx)
{
   HighResStatsList statsHistory;
   uint64_t lastStatsMS = getValue();

   // get stats history
   StatsCollector* statsCollector = Program::getApp()->getStatsCollector();
   statsCollector->getStatsSince(lastStatsMS, statsHistory);

   ctx.sendResponse(GetHighResStatsRespMsg(&statsHistory) );

   return true;
}

