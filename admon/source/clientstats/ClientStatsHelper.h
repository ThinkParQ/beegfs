#ifndef CLIENTSTATSHELPER_H_
#define CLIENTSTATSHELPER_H_

#include <common/Common.h>
#include <common/app/AbstractApp.h>
#include <clientstats/ClientStats.h>
#include <common/nodes/NodeStoreServers.h>



class ClientStatsHelper
{
   public:
      static bool getStatsFromNodes(NodeStoreServers* serverNodes, ClientStats* stats);
      static bool doClientStats(Node& node, ClientStats* stats);
      static bool getIOVec(Node& node, uint64_t cookieIP, bool requestPerUserStats,
         UInt64Vector* outVec);
};

#endif /* CLIENTSTATSHELPER_H_ */
