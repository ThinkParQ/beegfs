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
bool GetClientStatsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("GetClientStatsMsgEx incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a GetClientStatsMsg from: ") + peer);

   // From deserialization of the incoming buffer. We only want to send stats for clients with IP +1
   uint64_t cookieIP = getValue();

   // get stats
   MetaNodeOpStats* ClientOpStats = Program::getApp()->getClientOpStats();

   UInt64Vector opStatsVec;
   ClientOpStats->mapToUInt64Vec(cookieIP, bufLen, &opStatsVec);


   GetClientStatsRespMsg respMsg(&opStatsVec);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;
}

