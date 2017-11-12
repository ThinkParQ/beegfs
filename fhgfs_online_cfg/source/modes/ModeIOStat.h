#ifndef MODEIOSTAT_H_
#define MODEIOSTAT_H_

#include <common/Common.h>
#include <common/nodes/NodeStoreServers.h>
#include "Mode.h"


class ModeIOStat : public Mode
{
   public:
      ModeIOStat()
      {
         cfgPrintDetails = false;
         cfgHistorySecs = 10;
         cfgIntervalSecs = 0;
         cfgShowPerServer = false;
         cfgShowNames = false;
      }

      virtual int execute();

      static void printHelp();


   private:
      bool cfgPrintDetails;
      unsigned cfgHistorySecs; // number of last entries to show
      unsigned cfgIntervalSecs; // update interval
      bool cfgShowPerServer; // servers in rows (no history in this case)
      bool cfgShowNames; // show string names instead of numeric IDs (e.g. for servers)

      bool nodeStats(NodeStoreServers* nodes, uint16_t nodeID, NodeType nodeType);
      bool nodesTotalStats(NodeStoreServers* nodes, NodeType nodeType);
      bool perNodeStats(NodeStoreServers* nodes, NodeType nodeType);
      bool getIOStats(Node& node, HighResStatsList* outStats);
      void printStatsHistoryList(HighResStatsList* stats, NodeType nodeType);
      void printStatsNodeRow(uint64_t timeIndexMS, HighResStatsList& stats, Node& node,
         NodeType nodeType);
      void printStatsRowTitles(NodeType nodeType);
      void printStatsRow(HighResolutionStats& stats, NodeType nodeType);
      void addStatsToTotal(HighResStatsVec& totalVec, HighResStatsList& stats);

      void makeStdinNonblocking();
      void makeStdinBlocking();
      bool waitForInput(int timeoutSecs);
};

#endif /* MODEIOSTAT_H_ */
