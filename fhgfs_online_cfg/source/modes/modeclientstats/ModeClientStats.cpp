#include <common/nodes/OpCounter.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/TimeAbs.h>
#include <common/toolkit/UnitTk.h>
#include <common/clientstats/ClientStatsHelper.h>
#include <program/Program.h>
#include <modes/modehelpers/ModeInterruptedException.h>
#include "ModeClientStats.h"

#include <termios.h>
#include <poll.h>

#define MGMT_TIMEOUT_MS 2500 // time to connect to the mgmtd in ms

/**
 * Main entry method to get and print the statistics
 */
int ModeClientStats::execute()
{
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   NodeHandle mgmtNode;
   bool statsRes; // defined here already due to 'goto'

   // read parameters
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   int paramRes = this->cfgOptions.getParams(cfg);
   if (paramRes)
      return paramRes;

   int retVal = addNodesToNodeStore(app, mgmtNodes, mgmtNode);
   if (retVal != APPCODE_NO_ERROR)
      goto cleanup_mgmt;

   try
   {
      ModeHelper::registerInterruptSignalHandler();

      statsRes = receivePrintLoop(); // receive values and print results
   }
   catch (ModeInterruptedException& e)
   {
      // signal interrupted receivePrintLoop(), so restore it here
      makeStdinBlocking();
      statsRes = APPCODE_NO_ERROR;
   }

   retVal = statsRes ? APPCODE_NO_ERROR : APPCODE_RUNTIME_ERROR;

cleanup_mgmt:
   return retVal;
}

/**
 * get the nodeStore and add nodes to it
 */
int ModeClientStats::addNodesToNodeStore(App* app, NodeStoreServers* mgmtNodes,
   NodeHandle& outMgmtNode)
{
   DatagramListener* dgramLis = app->getDatagramListener();

   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();

   // check mgmt node
   const int mgmtTimeoutMS = MGMT_TIMEOUT_MS;
   if(!NodesTk::waitForMgmtHeartbeat(
      NULL, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP, mgmtTimeoutMS) )
   {
      std::cerr << "Management node communication failed: " << mgmtHost << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   outMgmtNode = mgmtNode;

   std::vector<NodeHandle> nodes;
   if (!NodesTk::downloadNodes(*mgmtNode, this->cfgOptions.nodeType, nodes, false, NULL) )
   {
      std::cerr << "Node download failed." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   // single user given node only?
   if (!this->cfgOptions.nodeID.empty())
   {
      uint16_t nodeNumID = StringTk::strToUInt(this->cfgOptions.nodeID);
      deleteListNodesExceptProvided(nodes, nodeNumID);
   }

   if (nodes.empty() && !this->cfgOptions.nodeID.empty())
   {
      std::cerr << "Node not found: \"" << this->cfgOptions.nodeID << "\"" << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }
   else
   if (nodes.empty() )
   {
      std::cerr << "Management node does not know about any nodes!" << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   NodeStoreServers* nodeStore;
   switch (this->cfgOptions.nodeType)
   {
      case NODETYPE_Meta:
         { nodeStore = app->getMetaNodes(); }; break;
      case NODETYPE_Storage:
         { nodeStore = app->getStorageNodes(); }; break;
      default:
      {
         std::cerr << "Only meta and storage nodetypes supported." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
      break;
   }

   NodesTk::applyLocalNicCapsToList(app->getLocalNode(), nodes);
   NodesTk::moveNodesFromListToStore(nodes, nodeStore);

   this->nodeStore = nodeStore;

   return APPCODE_NO_ERROR;
}

/**
 * Deletes all nodes in the list, but not the list itself and the provided node
 */
void ModeClientStats::deleteListNodesExceptProvided(std::vector<NodeHandle>& nodes,
   uint16_t exceptID)
{
   std::vector<NodeHandle> result;

   for (auto it = nodes.begin(), end = nodes.end(); it != end; ++it)
   {
      if ((*it)->getNumID() == NumNodeID(exceptID))
         result.push_back(std::move(*it));
   }

   nodes = std::move(result);
}


/**
 * Get and print values
 */
bool ModeClientStats::receivePrintLoop()
{
   makeStdinNonblocking();

   if(this->cfgOptions.intervalSecs)
   {
      std::cout << std::endl;
      std::cout << "          *************************" << std::endl;
      std::cout << "          * PRESS ANY KEY TO QUIT *" << std::endl;
      std::cout << "          *************************" << std::endl;
      std::cout << std::endl;

      ::sleep(1);
   }

   ClientStats stats(&this->cfgOptions);
   bool statsRes;
   uint64_t elapsedSecs = 0;

   do // infinite loop if refresh interval enabled (until key pressed)
   {
      statsRes = ClientStatsHelper::getStatsFromNodes(this->nodeStore, &stats);
      if (!statsRes)
      {
         makeStdinBlocking();
         return statsRes;
      }

      stats.printStats(elapsedSecs);

      if(waitForInput(this->cfgOptions.intervalSecs) )
      { // user pressed a key
         getchar(); // read the pressed key and ignore it
         break;
      }

      stats.currentToOldVectorMap();
      elapsedSecs += this->cfgOptions.intervalSecs;
   } while (this->cfgOptions.intervalSecs); /* cfgIntervalSecs will not change, but allows us
                                             * to run the loop for ever or a single time only */

   makeStdinBlocking();

   return statsRes;
}


/**
 * Just print the help
 */
void ModeClientStats::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  <nodeID>               The node to query." << std::endl;
   std::cout << "                         (Default: all nodes of type <nodetype>)" << std::endl;
   std::cout << "  --nodetype=<nodetype>  The node type to query (meta, storage)." << std::endl;
   std::cout << "                         (Default: meta)" << std::endl;
   std::cout << "  --interval=<secs>      Interval for repeated stats retrieval. Small intervals" << std::endl;
   std::cout << "                         might have a negative performance impact. interval=0" << std::endl;
   std::cout << "                         will exit after printing the absolute stats." << std::endl;
   std::cout << "                         (Default: 10)" << std::endl;
   std::cout << "  --maxlines=<lines>     Maximum number of output lines (clients/users)." << std::endl;
   std::cout << "                         (Default: 20)" << std::endl;
   std::cout << "  --allstats             Print all values. By default, zero values are skipped." << std::endl;
   std::cout << "  --rwunit=<unit>        Unit for read/write bytes. (Values: MiB, B)" << std::endl;
   std::cout << "                         (Default: MiB)" << std::endl;
   std::cout << "  --perinterval          Print storage read/write stats per given interval. By" << std::endl;
   std::cout << "                         default, these values are per second." << std::endl;
   std::cout << "  --names                Show hostnames instead of IPs and usernames instead of" << std::endl;
   std::cout << "                         numeric user IDs." << std::endl;
   std::cout << "  --filter=<name>        Show values for given client/user only." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode queries statistics for client or user requests from the servers and" << std::endl;
   std::cout << " presents them in a sorted list, ordered by sum of requests per client or user." << std::endl;
   std::cout << " This allows identification of those clients or users, which are currently" << std::endl;
   std::cout << " generating the most load on the servers." << std::endl;
   std::cout << std::endl;
   std::cout << " The initial batch of statistics for time index 0 shows the absolute number of" << std::endl;
   std::cout << " operations since the servers were started, so only the following batches show" << std::endl;
   std::cout << " values for the given interval." << std::endl;
   std::cout << std::endl;
   std::cout << " Note:" << std::endl;
   std::cout << "  Some client operation related messages (e.g. close file messages) are forwared" << std::endl;
   std::cout << "  by metadata servers. Thus, servers can also appear in the per-client stats." << std::endl;
   std::cout << "  For per-user stats, requests from unknown user IDs, e.g. due to request" << std::endl;
   std::cout << "  forwarding, will be shown as user ID \"-1\"." << std::endl;
   std::cout << std::endl;
   std::cout << " Note:" << std::endl;
   std::cout << "  User statistics are only available with version 2014.01-r6 or higher servers." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show per-client metadata access statistics, refresh every 5 seconds" << std::endl;
   std::cout << "  $ beegfs-ctl --clientstats --interval=5 --nodetype=meta" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show per-user storage access statistics, refresh every 5 seconds" << std::endl;
   std::cout << "  $ beegfs-ctl --userstats --names --interval=5 --nodetype=storage" << std::endl;
}

/**
 * We don't want to wait for stdin, but just want to check if the user pressed a key. So make
 * it non-blocking.
 */
void ModeClientStats::makeStdinNonblocking()
{
   struct termios terminalState;
   tcgetattr(STDIN_FILENO, &terminalState);

   terminalState.c_lflag &= ~(ICANON | ECHO); // turn off canonical mode

   terminalState.c_cc[VMIN] = 1; // min number of input returned

   tcsetattr(STDIN_FILENO, TCSANOW, &terminalState);
}

/**
 * Make stdin blocking again.
 */
void ModeClientStats::makeStdinBlocking()
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
bool ModeClientStats::waitForInput(int timeoutSecs)
{
   struct pollfd stdinPollFD;
   stdinPollFD.fd = STDIN_FILENO;
   stdinPollFD.events = POLLIN;

   int pollRes = poll(&stdinPollFD, 1, timeoutSecs*1000);

   return (pollRes != 0);
}
