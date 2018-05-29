#include <program/Program.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include "GetHighResStatsMsgEx.h"


bool GetHighResStatsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetHighResStatsMsgEx incoming";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a GetHighResStatsMsg from: " + ctx.peerName() );
   IGNORE_UNUSED_VARIABLE(logContext);

   HighResStatsList statsHistory;
   uint64_t lastStatsMS = getValue();

   // get stats history
   StatsCollector* statsCollector = Program::getApp()->getStatsCollector();
   statsCollector->getStatsSince(lastStatsMS, statsHistory);

   ctx.sendResponse(GetHighResStatsRespMsg(&statsHistory) );

   return true;
}

