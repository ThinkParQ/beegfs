#include <common/net/message/nodes/GetClientStatsMsg.h>
#include <common/net/message/nodes/GetClientStatsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include "ClientStatsHelper.h"


/**
 * Stats for all nodes or the node specified by the user
 *
 * @param serverNodes The servers to query, metadata servers or storage servers
 * @param stats The ClientStats to update
 */
bool ClientStatsHelper::getStatsFromNodes(NodeStoreServers* serverNodes, ClientStats* stats)
{
   std::string rootNodeID;
   bool retVal = true;

   for (const auto& node : serverNodes->referenceAllNodes())
   {
      // we ignore the return value as other nodes might be ok
      retVal = doClientStats(*node, stats);
   }

   return retVal;
}

/**
 * Stats for a single node.
 *
 * @return false on error
 */
bool ClientStatsHelper::doClientStats(Node& node, ClientStats* stats)
{
   bool getStatsRes;
   bool parseRes;

   // Loop to ask the server for stats data. Stop once we got data for all IPs
   do
   {
      UInt64Vector statsVec;
      uint64_t cookieIP;
      bool isPerUserStats = stats->getCfgOptions()->perUser;

      if (stats->moreData() )
         cookieIP = stats->getCookieIP();
      else
      {
         // As cookieIP is both, boolean if a cookie is set and also the real value,
         // as with have to deal with cookieIP=0 and as it is an unsigned, only ~0
         // is left to notify the server we do not have a cookie set
         cookieIP = ~0ULL;
      }

      getStatsRes = getIOVec(node, cookieIP, isPerUserStats, &statsVec);

      if (getStatsRes)
         parseRes = stats->parseStatsVector(&statsVec);
      else
         parseRes = false;

   } while (stats->moreData() && getStatsRes && parseRes);

   stats->resetFirstTransfer();

   if(!getStatsRes || !parseRes)
   {
      LogContext log = LogContext("ClientStats (collect stats) ");
      std::string logMessage("Getting stats failed for node: " + node.getNodeIDWithTypeStr());

      log.logErr(logMessage);
      std::cerr << logMessage << std::endl;
   }

   return parseRes;
}

/**
 * Get the vector that has the statistics from the server.
 *
 * @param requestPerUserStats true to request per-user statistics instead of default per-client
 * statistics.
 */
bool ClientStatsHelper::getIOVec(Node& node, uint64_t cookieIP, bool requestPerUserStats,
   UInt64Vector* outVec)
{
   LogContext log = LogContext("ClientStats (get IO vector) ");
   bool retVal = false;

   GetClientStatsRespMsg* respMsgCast;

   // we start to request a message for cookieIP=0, so no cookie set
   GetClientStatsMsg getStatsMsg(cookieIP);

   if(requestPerUserStats)
      getStatsMsg.addMsgHeaderFeatureFlag(GETCLIENTSTATSMSG_FLAG_PERUSERSTATS);

   const auto respMsg = MessagingTk::requestResponse(node, getStatsMsg,
         NETMSGTYPE_GetClientStatsResp);
   if(!respMsg)
   {
      std::string logMessage("Communication with node failed: " + node.getNodeIDWithTypeStr() );

      log.logErr(logMessage);
      std::cerr << logMessage << std::endl;
      goto err_cleanup;
   }

   respMsgCast = ((GetClientStatsRespMsg*)respMsg.get());
   respMsgCast->getStatsVector().swap(*outVec);

   retVal = true;

err_cleanup:
   return retVal;
}
