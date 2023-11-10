#include "ModeClientStats.h"

#include <common/nodes/OpCounter.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/TimeAbs.h>
#include <common/toolkit/UnitTk.h>
#include <modes/modehelpers/ModeInterruptedException.h>
#include <program/Program.h>

#include <iostream>
#include <termios.h>
#include <poll.h>

int ModeClientStats::execute()
{
   app = Program::getApp();

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   auto cfgIter = cfg->find("--nodetype");
   if (cfgIter != cfg->end())
   {
      if (cfgIter->second == "storage")
         nodeType = NODETYPE_Storage;
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->find("--peruser");
   if (cfgIter != cfg->end())
   {
      perUser = true;
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->find("--allstats");
   if (cfgIter != cfg->end())
   {
      allStats = true;
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->find("--interval");
   if (cfgIter != cfg->end())
   {
      interval = StringTk::strToUInt(cfgIter->second);
      cfg->erase(cfgIter);
   }

   diffLoop = interval > 0;

   cfgIter = cfg->find("--perinterval");
   if (cfgIter != cfg->end())
   {
      perInterval = true;
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->find("--maxlines");
   if (cfgIter != cfg->end())
   {
      maxLines = StringTk::strToUInt(cfgIter->second);
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->find("--filter");
   if (cfgIter != cfg->end())
   {
      filter = cfgIter->second;
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->find("--rwunit");
   if (cfgIter != cfg->end())
   {
      if (cfgIter->second == "B")
         rwUnitBytes = true;
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->find("--names");
   if (cfgIter != cfg->end())
   {
      names = true;
      cfg->erase(cfgIter);
   }

   cfgIter = cfg->begin();
   if (cfgIter != cfg->end())
   {
      nodeIDOnly = StringTk::strToUInt(cfgIter->first);
      if (nodeIDOnly == 0)
      {
         std::cerr << "Invalid NodeNumID given." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfg->erase(cfgIter);
   }

   if (!cfg->empty())
   {
      std::cerr << "Unknown config arguments." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   addNodesToVector();

   try
   {
      ModeHelper::registerInterruptSignalHandler();
      loop();
   }
   catch (ModeInterruptedException& e)
   {}

   makeStdinBlocking();
   return APPCODE_NO_ERROR;
}

/**
 * retrieve nodes and fill the nodeVector
 */
void ModeClientStats::addNodesToVector()
{
   NodeStoreServers* store = (nodeType == NODETYPE_Meta) ?
         app->getMetaNodes() : app->getStorageNodes();

   if (nodeIDOnly != 0)
   {
      NodeHandle result = store->referenceNode(NumNodeID(nodeIDOnly));
      if (result != nullptr)
         nodeVector.push_back(result);
   }
   else
      nodeVector = store->referenceAllNodes();
}


/**
 * Get and print values
 */
void ModeClientStats::loop()
{
   if (diffLoop)
   {
      makeStdinNonblocking();

      std::cout << std::endl;
      std::cout << "          *************************" << std::endl;
      std::cout << "          * PRESS ANY KEY TO QUIT *" << std::endl;
      std::cout << "          *************************" << std::endl;
      std::cout << std::endl;
   }

   uint64_t elapsedSecs = 0;

   do
   {
      bool opValuesClean = true;

      if(diffLoop)
         std::cout << std::endl << "====== " << elapsedSecs << " s ======" << std::endl;

      for (auto nodeIter = nodeVector.begin(); nodeIter != nodeVector.end(); nodeIter++)
      {
         auto idOpsUnorderedMap = ClientOpsRequestor::request(**nodeIter, perUser);
         for (auto mapIter = idOpsUnorderedMap.begin(); mapIter != idOpsUnorderedMap.end();
               mapIter++)
         {
            clientOps.addOpsList(mapIter->first, mapIter->second);
         }
      }

      ClientOps::IdOpsMap idOpsMap;
      ClientOps::OpsList sumOpsList;

      if (diffLoop && elapsedSecs > 0)
      {
         idOpsMap = clientOps.getDiffOpsMap();
         sumOpsList = clientOps.getDiffSumOpsList();
      }
      else
      {
         idOpsMap = clientOps.getAbsoluteOpsMap();
         sumOpsList = clientOps.getAbsoluteSumOpsList();
      }

     auto idOpsVector = sortedVectorByOpsSum(idOpsMap);

     if (!idOpsVector.empty())
      {
         if (filter.empty() && maxLines == 0)
         {
            const auto res = parseAndPrintOpsList("Sum:", sumOpsList);
            if (res == ParseResult::DIRTY)
               opValuesClean = false;
         }

         unsigned lineCounter = 0;
         auto opsMapIter = idOpsVector.begin();
         while (opsMapIter != idOpsVector.end() && (lineCounter < maxLines || maxLines == 0))
         {
            std::string id;
            if (perUser)
            {
               if (names)
                  id = System::getUserNameFromUID(opsMapIter->first);
               else
               {
                  // id is set to ~0 when userid can't be detected. printing -1 instead looks better
                  if (opsMapIter->first == (unsigned)~0)
                     id = "-1";
                  else
                     id = StringTk::uintToStr(opsMapIter->first);
               }
            }
            else
            {
               struct in_addr inAddr = { (in_addr_t)opsMapIter->first };

               if (names)
                  id = SocketTk::getHostnameFromIP(&inAddr, false, false);

               if (id.empty())
                  id = Socket::ipaddrToStr(inAddr);
            }

            if (filter.empty() || filter == id)
            {
               const auto res = parseAndPrintOpsList(id, opsMapIter->second);
               if (res == ParseResult::DIRTY)
                  opValuesClean = false;

               if (res != ParseResult::EMPTY)
                  lineCounter++;
            }

            opsMapIter++;
         }
      }

      if(!opValuesClean)
      {
         std::cout << "Warning: Operation counters changed unexpectedly."
               << "This is probably caused by a node going offline."
               << std::endl;
      }

      clientOps.clear();
      elapsedSecs += interval;

      if (!diffLoop)
         break;
   } while (!waitForInput(interval));

}

/**
 * Convert the map (key=user/client id, value=ops list) to a list sorted by sum of operations.
 * We want to print the operations table sorted by the sum.
 */
std::vector<std::pair<int64_t, ClientOps::OpsList>>
ModeClientStats::sortedVectorByOpsSum(const ClientOps::IdOpsMap &idOpsMap) const
{
   std::vector<std::pair<int64_t, ClientOps::OpsList>> idOpsList(idOpsMap.begin(), idOpsMap.end());
   std::stable_sort(idOpsList.begin(), idOpsList.end(),
         [](auto &v1, auto &v2) {
            if(v2.second.empty())
               return true;
            if(v1.second.empty())
               return false;
            return *v1.second.begin() > *v2.second.begin();
         }
   );
   return idOpsList;
}

/**
 * Parses and prints one list ops opCounters
 *
 * @return OK if all opValues are >= 0 and at least one > 0, EMPTY if all opValues are = 0,
 * and DIRTY if at least one is < 0 (e.g. when a diff is negative).
 */
ModeClientStats::ParseResult ModeClientStats::parseAndPrintOpsList(
      const std::string& id, const ClientOps::OpsList& opsList) const
{
   std::ostringstream buf;
   bool opValuesClean = true;

   unsigned opCounter = 0;
   for (auto listIter = opsList.begin(); listIter != opsList.end(); listIter++)
   {
      std::string opName;
      if (nodeType == NODETYPE_Meta)
         opName = OpToStringMapping::mapMetaOpNum(opCounter);
      else if (nodeType == NODETYPE_Storage)
         opName = OpToStringMapping::mapStorageOpNum(opCounter);

      if (*listIter != 0 || allStats)
      {
         // special treatment for bytes read and written
         if (nodeType == NODETYPE_Storage
               && (opName == OpToStringMapping::mapStorageOpNum(StorageOpCounter_WRITEBYTES)
               || opName == OpToStringMapping::mapStorageOpNum(StorageOpCounter_READBYTES)))
         {
            double amount = *listIter;
            if (!rwUnitBytes)
            {
               amount /= 1024*1024;
               opName = "Mi" + opName;
            }
            if (!perInterval && interval != 0)
            {
               amount /= interval;
               opName += "/s";
            }

            buf << amount << " [" << opName;
         }
         else
            buf << *listIter << " [" << opName;

         buf << "]  ";
      }

      opValuesClean &= *listIter >= 0;

      opCounter++;
   }

   if (!buf.str().empty())
   {
      std::cout << std::setw(16) << std::setiosflags(std::ios::left) << id << std::setw(1);
      std::cout << buf.str() << std::endl;

      return opValuesClean ? ParseResult::OK : ParseResult::DIRTY;
   }
   else
   {
      return ParseResult::EMPTY;
   }
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
   std::cout << "                         Set to 0 to print all lines. Enabling this will disable" << std::endl;
   std::cout << "                         the summarizing line." << std::endl;
   std::cout << "                         (Default: all)" << std::endl;
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
void ModeClientStats::makeStdinNonblocking() const
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
void ModeClientStats::makeStdinBlocking() const
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
bool ModeClientStats::waitForInput(int timeoutSecs) const
{
   struct pollfd stdinPollFD;
   stdinPollFD.fd = STDIN_FILENO;
   stdinPollFD.events = POLLIN;

   int pollRes = poll(&stdinPollFD, 1, timeoutSecs*1000);

   if (pollRes)
      getchar();

   return (pollRes != 0);
}
