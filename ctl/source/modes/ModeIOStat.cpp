#include <app/App.h>
#include <common/net/message/storage/GetHighResStatsMsg.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/TimeAbs.h>
#include <common/toolkit/UnitTk.h>
#include <modes/modehelpers/ModeInterruptedException.h>
#include <program/Program.h>
#include "ModeIOStat.h"

#include <termios.h>
#include <poll.h>


#define MODEIOSTAT_ARG_PRINTDETAILS     "--details"
#define MODEIOSTAT_ARG_HISTORYSECS      "--history"
#define MODEIOSTAT_ARG_INTERVALSECS     "--interval"
#define MODEIOSTAT_ARG_SHOWPERSERVER    "--perserver"
#define MODECLIENTSTATS_ARG_SHOWNAMES   "--names"
#define MODEIOSTAT_NUM_INACCURACY_SECS  (2) /* number of recent secs to remove from output */


int ModeIOStat::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   uint16_t nodeID = 0;

   NodeList nodes;

   // check arguments

   StringMapIter iter = cfg->find(MODEIOSTAT_ARG_PRINTDETAILS);
   if(iter != cfg->end() )
   {
      cfgPrintDetails = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOSTAT_ARG_HISTORYSECS);
   if(iter != cfg->end() )
   {
      unsigned newCfgHistorySecs = StringTk::strToUInt(iter->second);
      cfg->erase(iter);

      if( (newCfgHistorySecs <= 65) && (newCfgHistorySecs > 0) )
         cfgHistorySecs = newCfgHistorySecs;
   }

   iter = cfg->find(MODEIOSTAT_ARG_INTERVALSECS);
   if(iter != cfg->end() )
   {
      cfgIntervalSecs = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOSTAT_ARG_SHOWPERSERVER);
   if(iter != cfg->end() )
   {
      cfgShowPerServer = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODECLIENTSTATS_ARG_SHOWNAMES);
   if(iter != cfg->end() )
   {
      cfgShowNames = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODE_ARG_NODETYPE);
   if(iter == cfg->end() )
      cfg->insert(StringMapVal(MODE_ARG_NODETYPE, MODE_ARG_NODETYPE_STORAGE) ); // default nodetype

   NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg);
   if(nodeType == NODETYPE_Invalid)
   {
      std::cerr << "Invalid or missing node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else
   if( (nodeType != NODETYPE_Storage) && (nodeType != NODETYPE_Meta) &&
       (nodeType != NODETYPE_Mgmt) )
   {
      std::cerr << "Invalid node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   iter = cfg->begin();
   if(iter != cfg->end() )
   {
      bool isNumericRes = StringTk::isNumeric(iter->first);
      if(!isNumericRes)
      {
         std::cerr << "Invalid nodeID given (must be numeric): " << iter->first << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      nodeID = StringTk::strToUInt(iter->first);
   }

   NodeStoreServers* nodeStore;

   if(nodeType == NODETYPE_Meta)
      nodeStore = app->getMetaNodes();
   else
   if(nodeType == NODETYPE_Storage)
      nodeStore = app->getStorageNodes();
   else
      nodeStore = app->getMgmtNodes();

   // recv values and print results
   try
   {
      bool statsRes;

      ModeHelper::registerInterruptSignalHandler();

      makeStdinNonblocking();

      for( ; ; ) // infinite loop if refresh interval enabled (until key pressed)
      {
         if(!nodeID)
         {
            if(cfgShowPerServer)
               statsRes = perNodeStats(nodeStore, nodeType);
            else
               statsRes = nodesTotalStats(nodeStore, nodeType);
         }
         else
            statsRes = nodeStats(nodeStore, nodeID, nodeType);

         if(!statsRes || !cfgIntervalSecs)
            break;

         if(waitForInput(cfgIntervalSecs) )
         { // user pressed a key
            getchar(); // read the pressed key and ignore it
            break;
         }
      }

      retVal = statsRes ? APPCODE_NO_ERROR : APPCODE_RUNTIME_ERROR;
   }
   catch(ModeInterruptedException& e)
   {
      // nothing to be done here (we just need to catch this to do cleanup, e.g. of stdin)
   }

   makeStdinBlocking();

   return retVal;
}

void ModeIOStat::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  <nodeID>               Show only values of this server." << std::endl;
   std::cout << "                         (Default: Show aggregate values of all servers)" << std::endl;
   std::cout << "  --nodetype=<nodetype>  The node type to query (metadata, storage)." << std::endl;
   std::cout << "                         (Default: storage)" << std::endl;
   std::cout << "  --history=<secs>       Size of history length in seconds." << std::endl;
   std::cout << "                         (Default: 10; max: 60)" << std::endl;
   std::cout << "  --interval=<secs>      Interval for repeated stats retrieval in seconds." << std::endl;
   std::cout << "  --perserver            Show individual stats of all servers, one row per" << std::endl;
   std::cout << "                         server. (History is not available in this mode.)" << std::endl;
   std::cout << "  --names                Show names instead of only numeric server IDs if" << std::endl;
   std::cout << "                         \"--perserver\" option is used." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode shows the number of network requests that were processed per second" << std::endl;
   std::cout << " by the servers, the number of requests currently pending in the queue and the" << std::endl;
   std::cout << " number of worker threads that are currently busy processing requests at the" << std::endl;
   std::cout << " time of measurement on the servers, and the amount of read/written data per" << std::endl;
   std::cout << " second for storage servers." << std::endl;
   std::cout << std::endl;
   std::cout << " Unless \"--perserver\" is given, the output shows a history of the last few" << std::endl;
   std::cout << " seconds in rows with the most recent values at the bottom." << std::endl;
   std::cout << " The time_index field shows Unix timestamps in seconds since the epoch." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Print aggregate stats of metadata servers, refresh every 3 seconds." << std::endl;
   std::cout << "  $ beegfs-ctl --serverstats --nodetype=meta --interval=3" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Print individual stats of all storage servers, refresh every second." << std::endl;
   std::cout << "  $ beegfs-ctl --serverstats --nodetype=storage --perserver --names --interval=1" << std::endl;
}

/**
 * Stats for a single node.
 *
 * @return false on error
 */
bool ModeIOStat::nodeStats(NodeStoreServers* nodes, uint16_t nodeID, NodeType nodeType)
{
   bool retVal = false;

   auto node = nodes->referenceNode(NumNodeID(nodeID));
   if (!node)
   {
      std::cerr << "Unknown node: " << nodeID << std::endl;
      return false;
   }

   HighResStatsList stats;

   bool getStatsRes = getIOStats(*node, &stats);
   if(!getStatsRes)
      goto cleanup;

   while(stats.size() > cfgHistorySecs)
      stats.pop_back();

   printStatsHistoryList(&stats, nodeType);

   retVal = true;

cleanup:
   return retVal;
}

/**
 * Stats of all servers in row-wise history format.
 *
 * @return false on error
 */
bool ModeIOStat::nodesTotalStats(NodeStoreServers* nodes, NodeType nodeType)
{
   const uint64_t oneSecMS = 1000;

   HighResStatsVec statsTotal(cfgHistorySecs);

   uint64_t nowMS = TimeAbs().getTimeMS();

   uint64_t startMS = (nowMS - (nowMS % 1000) ); // round to 1000s
   startMS -= MODEIOSTAT_NUM_INACCURACY_SECS * 1000; // subtract inaccuracy time frame

   // init statsTotal time indices
   for(unsigned i=0; i < cfgHistorySecs; i++)
      statsTotal[i].rawVals.statsTimeMS = startMS - (i*oneSecMS);

   unsigned numStatNodes = 0;

   for (const auto& node : nodes->referenceAllNodes())
   {
      HighResStatsList stats;

      bool getStatsRes = getIOStats(*node, &stats);
      if(getStatsRes)
      { // add node stats to total stats history
         addStatsToTotal(statsTotal, stats);
         numStatNodes++;
      }
   }

   std::cout << "Total results for " << numStatNodes << " nodes:" << std::endl;

   HighResStatsList statsTotalList(statsTotal.begin(), statsTotal.end() );
   printStatsHistoryList(&statsTotalList, nodeType);

   return true;

}

/**
 * Stats of all servers, one row per server.
 *
 * @return false on error
 */
bool ModeIOStat::perNodeStats(NodeStoreServers* nodes, NodeType nodeType)
{
   uint64_t nowMS = TimeAbs().getTimeMS();

   uint64_t startMS = (nowMS - (nowMS % 1000) ); // round to 1000s
   startMS -= MODEIOSTAT_NUM_INACCURACY_SECS * 1000; // subtract inaccuracy time frame

   printf("\n");

   // time index headline

   if(cfgIntervalSecs > 1)
      printf("===== time index: %u (values show last second only) =====\n\n",
         (unsigned)(startMS/1000) );
   else
      printf("===== time index: %u =====\n\n",
         (unsigned)(startMS/1000) );

   // table headline

   printf("%6s ", "nodeID");

   printStatsRowTitles(nodeType);

   if(cfgShowNames)
      printf(" name");

   printf("\n");

   // get stats of each node and print the corresponding row of values

   for (const auto& node : nodes->referenceAllNodes())
   {
      NumNodeID nodeID = node->getNumID();
      HighResStatsList stats;

      bool getStatsRes = getIOStats(*node, &stats);
      if(getStatsRes)
      {
         printf("%6s ", nodeID.str().c_str());

         printStatsNodeRow(startMS, stats, *node, nodeType);

         if(cfgShowNames)
            printf(" %s", node->getID().c_str() );

         printf("\n");
      }
   }

   printf("\n");

   return true;
}

/**
 * Communicate with given node to retrieve its stats history.
 */
bool ModeIOStat::getIOStats(Node& node, HighResStatsList* outStats)
{
   bool retVal = false;

   GetHighResStatsRespMsg* respMsgCast;

   GetHighResStatsMsg getStatsMsg(0);

   const auto respMsg = MessagingTk::requestResponse(node, getStatsMsg,
         NETMSGTYPE_GetHighResStatsResp);
   if (!respMsg)
      goto err_cleanup;

   respMsgCast = (GetHighResStatsRespMsg*)respMsg.get();

   respMsgCast->getStatsList().swap(*outStats);

   retVal = true;

err_cleanup:
   return retVal;
}

/**
 * Print stats in row-wise history format (each second is a row, bottom row contains most recent
 * values).
 */
void ModeIOStat::printStatsHistoryList(HighResStatsList* stats, NodeType nodeType)
{
   printf("%11s ", "time_index");

   printStatsRowTitles(nodeType);

   printf("\n");

   for(HighResStatsListRevIter iter = stats->rbegin() ; iter != stats->rend(); iter++)
   {
      printf("%11u ", (unsigned)(iter->rawVals.statsTimeMS/1000) );

      printStatsRow(*iter, nodeType);

      printf("\n");
   }

   printf("\n");
}

/**
 * Print stats of given node in a row.
 */
void ModeIOStat::printStatsNodeRow(uint64_t timeIndexMS, HighResStatsList& stats, Node& node,
   NodeType nodeType)
{
   const uint64_t oneSecMS = 1000;

   HighResStatsList zeroStatsList(1); // empty dummy if node stats don't contain needed time index

   HighResStatsListIter statsIter = stats.begin();

   while(statsIter != stats.end() )
   {
      if(statsIter->rawVals.statsTimeMS >= (timeIndexMS + oneSecMS) )
      { // not matching time index, try next history entry
         statsIter++;
         continue;
      }

      // we found a matching time index
      break;
   }

   if(statsIter == stats.end() )
   { // stats history didn't contain matching time index => use dummy with zero values

      statsIter = zeroStatsList.begin();
   }

   printStatsRow(*statsIter, nodeType);
}

/**
 * Print header with titles for stats row.
 *
 * Note: Keep columns in sync with printStatsRow().
 */
void ModeIOStat::printStatsRowTitles(NodeType nodeType)
{
   if(!cfgPrintDetails)
   { // short output

      if(nodeType == NODETYPE_Storage)
      { // storage
         printf("%9s %9s %6s %6s %3s",
            "write_KiB", "read_KiB", "reqs", "qlen", "bsy");

      }
      else
      { // meta/mgmtd
         printf("%6s %6s %3s",
            "reqs", "qlen", "bsy");
      }

   }
   else
   { // detailed output (print all available columns)
      printf("%9s %9s %8s %8s %6s %6s %3s",
         "write_KiB", "read_KiB", "recv_KiB", "send_KiB", "reqs", "qlen", "bsy");
   }
}

/**
 * Print a stats row, so each value is a different column. Caller will typically want to add
 * time_index or nodeID as an extra first column.
 *
 * Note: Keep columns in sync with printStatsRowTitles().
 */
void ModeIOStat::printStatsRow(HighResolutionStats& stats, NodeType nodeType)
{
   if(!cfgPrintDetails)
   { // short output

      if(nodeType == NODETYPE_Storage)
      { // storage
         printf("%9.0f %9.0f %6u %6u %3u",
            stats.incVals.diskWriteBytes / 1024.0, stats.incVals.diskReadBytes / 1024.0,
            stats.incVals.workRequests, stats.rawVals.queuedRequests, stats.rawVals.busyWorkers);
      }
      else
      { // meta/mgmtd
         printf("%6u %6u %3u",
            stats.incVals.workRequests, stats.rawVals.queuedRequests, stats.rawVals.busyWorkers);
      }

   }
   else
   { // detailed output (print all available columns)

      printf("%9.0f %9.0f %8.0f %8.0f %6u %6u %3u",
         stats.incVals.diskWriteBytes / 1024.0, stats.incVals.diskReadBytes / 1024.0,
         stats.incVals.netRecvBytes / 1024.0, stats.incVals.netSendBytes / 1024.0,
         stats.incVals.workRequests, stats.rawVals.queuedRequests, stats.rawVals.busyWorkers);
   }
}

void ModeIOStat::addStatsToTotal(HighResStatsVec& totalVec, HighResStatsList& stats)
{
   const uint64_t oneSecMS = 1000;

   size_t totalIdx = 0;
   HighResStatsListIter statsIter = stats.begin();

   while( (totalIdx < totalVec.size() ) && (statsIter != stats.end() ) )
   {
      if(statsIter->rawVals.statsTimeMS >= (totalVec[totalIdx].rawVals.statsTimeMS + oneSecMS) )
         statsIter++;
      else
      if(statsIter->rawVals.statsTimeMS < totalVec[totalIdx].rawVals.statsTimeMS)
         totalIdx++;
      else
      {
         HighResolutionStatsTk::addHighResIncStats(*statsIter, totalVec[totalIdx]);
         HighResolutionStatsTk::addHighResRawStats(*statsIter, totalVec[totalIdx]);

         statsIter++;
      }
   }
}

void ModeIOStat::makeStdinNonblocking()
{
   struct termios terminalState;
   tcgetattr(STDIN_FILENO, &terminalState);

   terminalState.c_lflag &= ~(ICANON | ECHO); // turn off canonical mode

   terminalState.c_cc[VMIN] = 1; // min number of input returned

   tcsetattr(STDIN_FILENO, TCSANOW, &terminalState);
}

void ModeIOStat::makeStdinBlocking()
{
   struct termios terminalState;
   tcgetattr(STDIN_FILENO, &terminalState);

   terminalState.c_lflag |= (ICANON | ECHO); // turn on canonical mode

   tcsetattr(STDIN_FILENO, TCSANOW, &terminalState);
}

/**
 * Wait for available data on stdin.
 *
 * @return true if data available or error, false otherwise
 */
bool ModeIOStat::waitForInput(int timeoutSecs)
{
   struct pollfd stdinPollFD;
   stdinPollFD.fd = STDIN_FILENO;
   stdinPollFD.events = POLLIN;

   int pollRes = poll(&stdinPollFD, 1, timeoutSecs*1000);

   return (pollRes != 0);
}
