#include <app/App.h>
#include <common/net/message/nodes/StorageBenchControlMsg.h>
#include <common/net/message/nodes/StorageBenchControlMsgResp.h>
#include <common/nodes/TargetMapper.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeStorageBench.h"


#define STORAGEBENCH_PERFORMANCE_UNIT                " KiB/s"

//the arguments of the storage benchmark
#define MODESTORAGEBENCH_ARG_TARGET_IDS              "--targetids"
#define MODESTORAGEBENCH_ARG_ALL_TARGETS             "--alltargets"
#define MODESTORAGEBENCH_ARG_SERVER                  "--servers"
#define MODESTORAGEBENCH_ARG_TYPE_READ               "--read"
#define MODESTORAGEBENCH_ARG_TYPE_WRITE              "--write"
#define MODESTORAGEBENCH_ARG_BLOCK_SIZE              "--blocksize"
#define MODESTORAGEBENCH_ARG_SIZE                    "--size"
#define MODESTORAGEBENCH_ARG_ACTION_STOP             "--stop"
#define MODESTORAGEBENCH_ARG_ACTION_STATUS           "--status"
#define MODESTORAGEBENCH_ARG_ACTION_CLEANUP          "--cleanup"
#define MODESTORAGEBENCH_ARG_THREADS                 "--threads"
#define MODESTORAGEBENCH_ARG_VERBOSE                 "--verbose"
#define MODESTORAGEBENCH_ARG_WAIT                    "--wait"
#define MODESTORAGEBENCH_ARG_ODIRECT                 "--odirect"


/**
 * @param: APPCODE_...
 */
int ModeStorageBench::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   retVal = checkConfig(*mgmtNode, storageNodes, cfg);
   if (retVal != APPCODE_NO_ERROR)
      return retVal;

   UInt16List targetIds;
   NumNodeIDList nodeIDs;
   this->cfgTargetIDs.getMappingAsLists(targetIds, nodeIDs);

   //remove duplicate nodeIDs
   nodeIDs.sort();
   nodeIDs.unique();

   StorageBenchResponseInfoList responses;

   //send command to the storage servers and collect the responses
   for (NumNodeIDListIter iter = nodeIDs.begin(); iter != nodeIDs.end(); iter++)
   {
      if (!storageNodes->isNodeActive(*iter))
      {
         std::cerr << "Storage server " << storageNodes->getTypedNodeID(*iter)
            << " not found or not active." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      StorageBenchResponseInfo response;

      if (!sendCmdAndCollectResult(*iter, response))
         return APPCODE_RUNTIME_ERROR;

      responses.push_back(response);
   }

   if(this->cfgWait)
   { // poll the servers to check if all benchmarks finished

      responses.clear();

      // set the storage bench action to status
      this->cfgAction = StorageBenchAction_STATUS;

      bool benchmarksRunning = true;

      while(benchmarksRunning)
      {
         for (NumNodeIDListIter iter = nodeIDs.begin(); iter != nodeIDs.end(); iter++)
         {
            if (!storageNodes->isNodeActive(*iter))
            {
               std::cerr << "Storage server " << storageNodes->getTypedNodeID(*iter)
                  << " not found or not active." << std::endl;
               return APPCODE_RUNTIME_ERROR;
            }

            StorageBenchResponseInfo response;

            if (!sendCmdAndCollectResult(*iter, response))
               return APPCODE_RUNTIME_ERROR;

            responses.push_back(response);
         }

         benchmarksRunning = false;

         // check if all benchmarks are not running
         for (StorageBenchResponseInfoListIter iter = responses.begin(); iter != responses.end();
            iter++)
         {
            if(iter->status == StorageBenchStatus_RUNNING ||
               iter->status == StorageBenchStatus_FINISHING ||
               iter->status == StorageBenchStatus_STOPPING)
            {
               benchmarksRunning = true;
               break;
            }
         }

         if(!benchmarksRunning)
            break;

         responses.clear();

         // wait a few seconds before trying again
         ::sleep(5);
      }
   }

   //print the response
   printResults(&responses);

   return retVal;
}

/*
 * checks the arguments from the commandline
 *
 * @param mgmtNode the managment node
 * @param storageNodes a NodeStore with all storage servers
 * @param cfg a mstring map with all arguments from the command line
 * @return the error code (APPCODE_...)
 *
 */
int ModeStorageBench::checkConfig(Node& mgmtNode, NodeStoreServers* storageNodes, StringMap* cfg)
{
   int retVal = APPCODE_INVALID_CONFIG;
   StringMapIter iter;

   // check arguments
   // parse action argument stop
   iter = cfg->find(MODESTORAGEBENCH_ARG_ACTION_STOP);
   if (iter != cfg->end())
   {
      if (this->cfgType == StorageBenchType_NONE && this->cfgAction == StorageBenchAction_NONE)
      {
         this->cfgAction = StorageBenchAction_STOP;
         cfg->erase(iter);
      }
      else
      {
         std::cerr << "Invalid configuration. Only one of --read, --write, --stop, --status or "
            "--cleanup is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }

   // parse action arguments status
   iter = cfg->find(MODESTORAGEBENCH_ARG_ACTION_STATUS);
   if (iter != cfg->end())
   {
      if (this->cfgType == StorageBenchType_NONE && this->cfgAction == StorageBenchAction_NONE)
      {
         this->cfgAction = StorageBenchAction_STATUS;
         cfg->erase(iter);
      }
      else
      {
         std::cerr << "Invalid configuration. Only one of --read, --write, --stop, --status or "
            "--cleanup is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }

   // parse action argument cleanup
   iter = cfg->find(MODESTORAGEBENCH_ARG_ACTION_CLEANUP);
   if (iter != cfg->end())
   {
      if (this->cfgType == StorageBenchType_NONE && this->cfgAction == StorageBenchAction_NONE)
      {
         this->cfgAction = StorageBenchAction_CLEANUP;
         cfg->erase(iter);
      }
      else
      {
         std::cerr << "Invalid configuration. Only one of --read, --write, --stop, --status or "
            "--cleanup is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }

   // parse type argument read
   iter = cfg->find(MODESTORAGEBENCH_ARG_TYPE_READ);
   if(iter != cfg->end() )
   {
      if (this->cfgType == StorageBenchType_NONE && this->cfgAction == StorageBenchAction_NONE)
      {
         this->cfgType = StorageBenchType_READ;
         this->cfgAction = StorageBenchAction_START;
         cfg->erase(iter);
      }
      else
      {
         std::cerr << "Invalid configuration. Only one of --read, --write, --stop, --status or "
            "--cleanup is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }

   // parse type argument write
   iter = cfg->find(MODESTORAGEBENCH_ARG_TYPE_WRITE);
   if(iter != cfg->end() )
   {
      if (this->cfgType == StorageBenchType_NONE && this->cfgAction == StorageBenchAction_NONE)
      {
         this->cfgType = StorageBenchType_WRITE;
         this->cfgAction = StorageBenchAction_START;
         cfg->erase(iter);
      }
      else
      {
         std::cerr << "Invalid configuration. Only one of --read, --write, --stop, --status or "
            "--cleanup is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }

   // check if one action argument was set
   if (this->cfgAction == StorageBenchAction_NONE || this->cfgAction == StorageBenchAction_NONE)
   {
      std::cerr << "Invalid configuration. One of --read, --write, --stop, --status or "
         "--cleanup is required." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }



   // parse target argument targetIDs
   iter = cfg->find(MODESTORAGEBENCH_ARG_TARGET_IDS);
   if( (iter != cfg->end()) && (cfgTargetIDs.getSize() == 0) && (!iter->second.empty()) )
   {
      StringList targetIDs;
      StringTk::explode(iter->second, ',', &targetIDs);

      auto mappings = NodesTk::downloadTargetMappings(mgmtNode, false);

      // Delete all mappings for targets that are not in targetIDs, i.e. given by the user.
      // Also deletes targetIDs entries to check for unknown targets.
      for (auto it = mappings.second.begin(); it != mappings.second.end();)
      {
         bool targetFound = false;

         for(StringListIter inputTargetIdIter = targetIDs.begin();
            inputTargetIdIter != targetIDs.end(); inputTargetIdIter++)
         {
            if (StringTk::strToUInt(*inputTargetIdIter) == it->first)
            {
               targetFound = true;
               targetIDs.erase(inputTargetIdIter);
               break;
            }
         }

         if (targetFound)
            it++;
         else
            it = mappings.second.erase(it);
      }

      if (!targetIDs.empty() )
      {
         for (StringListIter iter = targetIDs.begin(); iter != targetIDs.end(); iter++)
         {
            std::cerr << "Unkown targetID: " << *iter << std::endl;
         }
         return APPCODE_INVALID_CONFIG;
      }

      this->cfgTargetIDs.syncTargets(std::move(mappings.second));
      cfg->erase(iter);
   }

   // parse target argument allTargets
   iter = cfg->find(MODESTORAGEBENCH_ARG_ALL_TARGETS);
   if( (iter != cfg->end()) && (cfgTargetIDs.getSize() == 0) )
   {
      this->cfgAllTargets = true;

      cfgTargetIDs.syncTargets(NodesTk::downloadTargetMappings(mgmtNode, false).second);

      cfg->erase(iter);
   }

   // parse target argument servers
   iter = cfg->find(MODESTORAGEBENCH_ARG_SERVER);
   if( (iter != cfg->end()) && (cfgTargetIDs.getSize() == 0) && (!iter->second.empty()) )
   {
      StringTk::explode(iter->second, ',', &this->cfgStorageServer);
      bool nodeNotFound = false;

      for(StringListIter storageIter = this->cfgStorageServer.begin();
          storageIter != this->cfgStorageServer.end();
          storageIter++ )
      {
         if(!storageNodes->isNodeActive(NumNodeID(StringTk::strToUInt(*storageIter) ) ) )
         {
            std::cerr << "Storage server " << *storageIter << " not found or not active."
               << std::endl;
            nodeNotFound = true;
         }
      }

      if (nodeNotFound)
      {
         return APPCODE_INVALID_CONFIG;
      }

      auto mappings = NodesTk::downloadTargetMappings(mgmtNode, false);

      for (auto it = mappings.second.begin(); it != mappings.second.end();)
      {
         bool targetFound = false;

         for(StringListIter inputNodeIter = this->cfgStorageServer.begin();
            inputNodeIter != this->cfgStorageServer.end();
            inputNodeIter++)
         {
            if (it->second == NumNodeID(StringTk::strToUInt(*inputNodeIter)))
            {
               targetFound = true;
               break;
            }
         }

         if (targetFound)
            it++;
         else
            it = mappings.second.erase(it);
      }

      this->cfgTargetIDs.syncTargets(std::move(mappings.second));
      cfg->erase(iter);
   }

   // check if some targets are given
   if ((cfgTargetIDs.getSize() == 0))
   {
      std::cerr << "Invalid configuration. No targets selected." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }



   // parse blocksize argument
   iter = cfg->find(MODESTORAGEBENCH_ARG_BLOCK_SIZE);
   if( (iter != cfg->end()) && (!iter->second.empty()) )
   {
      this->cfgBlocksize = UnitTk::strHumanToInt64(iter->second);
      cfg->erase(iter);
   }

   // parse size argument
   iter = cfg->find(MODESTORAGEBENCH_ARG_SIZE);
   if( (iter != cfg->end()) && (!iter->second.empty()) )
   {
      this->cfgSize = UnitTk::strHumanToInt64(iter->second);
      cfg->erase(iter);
   }

   // parse threads argument
   iter = cfg->find(MODESTORAGEBENCH_ARG_THREADS);
   if( (iter != cfg->end()) && (!iter->second.empty()) )
   {
      this->cfgThreads = StringTk::strToInt(iter->second);
      cfg->erase(iter);
   }

   // check if the required arguments are given or default values available
   if( (this->cfgAction == StorageBenchAction_START) &&
      ((this->cfgBlocksize == 0) || (this->cfgSize == 0) || (this->cfgThreads == 0)) )
   {
      std::cerr << "Invalid configuration. The --blocksize, --size and --threads option must "
         "be > 0." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }



   // parse verbose argument
   iter = cfg->find(MODESTORAGEBENCH_ARG_VERBOSE);
   if(iter != cfg->end())
   {
      this->cfgVerbose = true;
      cfg->erase(iter);
   }



   // parse wait argument
   iter = cfg->find(MODESTORAGEBENCH_ARG_WAIT);
   if(iter != cfg->end())
   {
      this->cfgWait = true;
      cfg->erase(iter);
   }



   // parse odirect argument
   iter = cfg->find(MODESTORAGEBENCH_ARG_ODIRECT);
   if(iter != cfg->end())
   {
      this->cfgODirect = true;
      cfg->erase(iter);
   }


   //check if wrong arguments are given
   if( ModeHelper::checkInvalidArgs(cfg) )
   {
      retVal = APPCODE_INVALID_CONFIG;
   }
   else
   {
      retVal = APPCODE_NO_ERROR;
   }

   return retVal;
}

/*
 * prints some results: fastest and slowest target, average throughput, throughput over all targets
 *
 * @param responses a list with the content of all response massages
 *
 */
void ModeStorageBench::printShortResults(StorageBenchResponseInfoList* responses)
{
   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   int64_t fastestPerformance = 0;
   int64_t slowestPerformance = 0;

   NumNodeID fastestNodeID;
   NumNodeID slowestNodeID;

   std::string fastestNodeIDStr;
   std::string slowestNodeIDStr;

   uint16_t fastestTargetID = 0; // silence gcc
   uint16_t slowestTargetID = 0; // silence gcc

   int64_t maxPerformance = 0;
   int64_t storageTargetCount = 0;

   for (StorageBenchResponseInfoListIter iter = responses->begin(); iter != responses->end();
      iter++)
   {
      if( (iter->status != StorageBenchStatus_ERROR) &&
          (iter->status != StorageBenchStatus_UNINITIALIZED) &&
          (iter->status != StorageBenchStatus_INITIALISED) )
      {
         for(StorageBenchResultsMapIter resultIter = iter->results.begin();
             resultIter != iter->results.end();
             resultIter++)
         {
            maxPerformance += resultIter->second;
            storageTargetCount++;

            if (resultIter->second > fastestPerformance)
            {
               fastestPerformance = resultIter->second;
               fastestNodeID = iter->nodeID;
               fastestNodeIDStr = storageNodes->getTypedNodeID(fastestNodeID);
               fastestTargetID = resultIter->first;
            }

            if ( (resultIter->second < slowestPerformance) || (slowestPerformance == 0) )
            {
               slowestPerformance = resultIter->second;
               slowestNodeID = iter->nodeID;
               slowestNodeIDStr = storageNodes->getTypedNodeID(slowestNodeID);
               slowestTargetID = resultIter->first;
            }
         }
      }
   }

   if (slowestPerformance != 0)
   {
      printf("%-23s %10" PRId64 " %-8s nodeID: %s, targetID: %u\n", "Min throughput:",
         slowestPerformance, STORAGEBENCH_PERFORMANCE_UNIT, slowestNodeIDStr.c_str(),
         slowestTargetID);
   }

   if (fastestPerformance != 0)
   {
      printf("%-23s %10" PRId64 " %-8s nodeID: %s, targetID: %u\n", "Max throughput:",
         fastestPerformance, STORAGEBENCH_PERFORMANCE_UNIT, fastestNodeIDStr.c_str(),
         fastestTargetID);
   }

   if ( (storageTargetCount != 0) && (maxPerformance != 0) )
   {
      printf("%-23s %10" PRId64 " %s\n", "Avg throughput:", maxPerformance / storageTargetCount,
         STORAGEBENCH_PERFORMANCE_UNIT);
   }

   if (maxPerformance != 0)
   {
      printf("%-23s %10" PRId64 " %s\n\n", "Aggregate throughput:", maxPerformance,
         STORAGEBENCH_PERFORMANCE_UNIT);
   }
}

/*
 * prints the results of all targets
 *
 * @param responses a list with the content of all response massages
 *
 */
void ModeStorageBench::printVerboseResults(StorageBenchResponseInfoList* responses)
{
   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   std::cout << "List of all targets:" << std::endl;

   for (StorageBenchResponseInfoListIter iter = responses->begin(); iter != responses->end();
      iter++)
   {
      if( (iter->status != StorageBenchStatus_ERROR) &&
          (iter->status != StorageBenchStatus_UNINITIALIZED) &&
          (iter->status != StorageBenchStatus_INITIALISED) )
      {
         for(StorageBenchResultsMapIter resultIter = iter->results.begin();
             resultIter != iter->results.end();
             resultIter++)
         {
            std::string nodeIDStr = storageNodes->getTypedNodeID(iter->nodeID);

            printf("%-23hu %10" PRId64 " %-8s nodeID: %s\n", resultIter->first, resultIter->second,
               STORAGEBENCH_PERFORMANCE_UNIT, nodeIDStr.c_str());
         }
      }
   }
}

/*
 * analyze the responses from the storage servers and prints the given informations
 *
 * @param responses a list with the content of all response massages
 *
 */
void ModeStorageBench::printResults(StorageBenchResponseInfoList* responses)
{
   StorageBenchAction action = responses->front().action;
   StorageBenchType type = responses->front().type;

   bool erroroccurred = checkAndPrintCurrentStatus(responses, false);

   //check if a error occurred
   for (StorageBenchResponseInfoListIter iter = responses->begin(); iter != responses->end();
      iter++)
   {
      if (iter->errorCode != STORAGEBENCH_ERROR_NO_ERROR)
      {
         erroroccurred = true;
      }
   }

   std::cout << std::endl;

   if(erroroccurred)
   {
      printError(responses);
      checkAndPrintCurrentStatus(responses, true);
   }
   else
   {
      switch (action)
      {
         case StorageBenchAction_START:
         {
            if (type == StorageBenchType_READ)
            {
               std::cout << "Read storage benchmark was started." << std::endl;
            }
            if (type == StorageBenchType_WRITE)
            {
               std::cout << "Write storage benchmark was started." << std::endl;
            }

            std::cout << "You can query the status with the --status argument of beegfs-ctl.";
            std::cout << std::endl;
            std::cout << std::endl;

            checkAndPrintCurrentStatus(responses, true);

         } break;
         case StorageBenchAction_STOP:
         {
            std::cout << "Stopping storage benchmark. This will take a few moments." << std::endl;
            std::cout << "You can query the status and the results with the --status argument " <<
               "of beegfs-ctl." << std::endl;
            std::cout << std::endl;

            checkAndPrintCurrentStatus(responses, true);

         } break;
         case StorageBenchAction_STATUS:
         {
            checkAndPrintCurrentStatus(responses, true);
            std::cout << std::endl;

            if (type == StorageBenchType_READ)
            {
               std::cout << "Read benchmark results:" << std::endl;
            }
            else
            if (type == StorageBenchType_WRITE)
            {
               std::cout << "Write benchmark results:" << std::endl;
            }

            printShortResults(responses);
            if(this->cfgVerbose)
            {
               printVerboseResults(responses);
            }
         } break;
         case StorageBenchAction_CLEANUP:
         {
            std::cout << "Cleanup storage benchmark folder." << std::endl;
            std::cout << std::endl;

            checkAndPrintCurrentStatus(responses, true);
         } break;
         default:
         {
            std::cerr << "Error: unknown action!" << std::endl;
         } break;
      }
   }

   std::cout << std::endl;
}

/*
 * analyse the status of the storage servers and prints the informations
 *
 * @param responses a list with the content of all response massages
 * @param true for printing the status to std::cout, false for only calculate the return value
 * @return error information, true if a error occurred, if not false returned
 *
 */
bool ModeStorageBench::checkAndPrintCurrentStatus(StorageBenchResponseInfoList* responses,
   bool printStatus)
{
   bool erroroccurred = false;
   int countOfNotInitialized = 0;
   int countOfInitialized = 0;
   int countOferrors = 0;
   int countOfRunning = 0;
   int countOfStopping = 0;
   int countOfStopped = 0;
   int countOfFinishing = 0;
   int countOfFisished = 0;

   //count the status of the storage servers
   for (StorageBenchResponseInfoListIter iter = responses->begin(); iter != responses->end();
      iter++)
   {
      switch (iter->status)
      {
         case StorageBenchStatus_UNINITIALIZED:
         {
            countOfNotInitialized++;
         } break;
         case StorageBenchStatus_INITIALISED:
         {
            countOfInitialized++;
         } break;
         case StorageBenchStatus_ERROR:
         {
            countOferrors++;
         } break;
         case StorageBenchStatus_RUNNING:
         {
            countOfRunning++;
         } break;
         case StorageBenchStatus_STOPPING:
         {
            countOfStopping++;
         } break;
         case StorageBenchStatus_STOPPED:
         {
            countOfStopped++;
         } break;
         case StorageBenchStatus_FINISHING:
         {
            countOfFinishing++;
         } break;
         case StorageBenchStatus_FINISHED:
         {
            countOfFisished++;
         } break;
         default:
         {
            std::cerr << "Error: Unknown status." << std::endl;
         } break;
      }
   }



   if (printStatus)
   {
      std::cout << "Server benchmark status:" << std::endl;

      if ( (countOfNotInitialized != 0) || (countOfInitialized) )
      {
         printf("%-12s %2d\n", "Didn't run: ", countOfNotInitialized + countOfInitialized);
      }

      if (countOferrors != 0)
      {
         printf("%-12s %2d\n", "Error: ", countOferrors);
      }

      if (countOfRunning != 0)
      {
         printf("%-12s %2d\n", "Running: ", countOfRunning);
      }

      if ( (countOfStopping != 0) || (countOfFinishing != 0) )
      {
         printf("%-12s %2d\n", "Stopping: ", countOfStopping + countOfFinishing);
      }

      if (countOfStopped != 0)
      {
         printf("%-12s %2d\n", "Stopped: ", countOfStopped);
      }

      if (countOfFisished != 0)
      {
         printf("%-12s %2d\n", "Finished: ", countOfFisished);
      }
   }

   return erroroccurred;
}

/*
 * analyze the errors of the storage servers and prints the informations
 *
 * @param responses a list with the content of all response massages
 *
 */
void ModeStorageBench::printError(StorageBenchResponseInfoList* responses)
{
   std::cerr << "Some errors occurred:" << std::endl;

   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();

   for (StorageBenchResponseInfoListIter iter = responses->begin(); iter != responses->end();
      iter++)
   {
      std::string nodeIDStr = storageNodes->getTypedNodeID(iter->nodeID);
      switch (iter->errorCode)
      {
         case STORAGEBENCH_ERROR_WORKER_ERROR:
         {
            std::cerr << nodeIDStr << ": I/O operation on disk failed." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_NO_ERROR:
         {
            std::cerr << nodeIDStr << ": Command executed. No error." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_UNINITIALIZED:
         {
            std::cerr << nodeIDStr << ": Benchmark uninitialized." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_INITIALIZATION_ERROR:
         {
            std::cerr << nodeIDStr << ": Initialization error." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_INIT_READ_DATA:
         {
            std::cerr << nodeIDStr << ": No (or not enough) data for read benchmark available." <<
               std::endl;
         } break;
         case STORAGEBENCH_ERROR_INIT_CREATE_BENCH_FOLDER:
         {
            std::cerr << nodeIDStr << ": Couldn't create the benchmark folder." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_INIT_TRANSFER_DATA:
         {
            std::cerr << nodeIDStr << ": Couldn't initialize random data." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_RUNTIME_ERROR:
         {
            std::cerr << nodeIDStr << ": Runtime error." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_RUNTIME_DELETE_FOLDER:
         {
            std::cerr << nodeIDStr << ": Unable to delete benchmark files." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_RUNTIME_OPEN_FILES:
         {
            std::cerr << nodeIDStr << ": Couldn't open benchmark file." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_RUNTIME_UNKNOWN_TARGET:
         {
            std::cerr << nodeIDStr << ": TargetID unknown." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_RUNTIME_IS_RUNNING:
         {
            std::cerr << nodeIDStr << ": Couldn't start a second benchmark." << std::endl;
         } break;
         case STORAGEBENCH_ERROR_RUNTIME_CLEANUP_JOB_ACTIVE:
         {
            std::cerr << nodeIDStr << ": Couldn't cleanup benchmark folder." << std::endl;
         } break;
         default:
         {
            std::cerr << "Error: " + nodeIDStr + ": Unknown error." << std::endl;
         } break;
      }
   }

   std::cerr << "For more details look into the storage server log file on the mentioned "
      "storage server." << std::endl;
   std::cerr << std::endl;
}

/*
 * prints the help
 */
void ModeStorageBench::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  One of these arguments is mandatory:" << std::endl;
   std::cout << "    --alltargets              Benchmark all registered storage targets." << std::endl;
   std::cout << "    --targetids=<targetlist>  Comma-separated list of numeric targetIDs." << std::endl;
   std::cout << "                              Benchmark only the given storage targets." << std::endl;
   std::cout << "    --servers=<nodelist>      Comma-separated list of numeric nodeIDs. Benchmark" << std::endl;
   std::cout << "                              all storage targets of the given servers." << std::endl;
   std::cout << std::endl;
   std::cout << "  One of these arguments is mandatory:" << std::endl;
   std::cout << "    --write    Start a write benchmark." << std::endl;
   std::cout << "    --read     Start a read benchmark (requires previous write benchmark)." << std::endl;
   std::cout << "    --stop     Stop a running benchmark." << std::endl;
   std::cout << "    --status   Print status/results of a benchmark." << std::endl;
   std::cout << "    --cleanup  Delete the benchmark files from the storage targets." << std::endl;
   std::cout << std::endl;
   std::cout << " Recommended:" << std::endl;
   std::cout << "    --blocksize=<blocksize>   The read/write blocksize for the benchmark." << std::endl;
   std::cout << "                              (Default: 128K)" << std::endl;
   std::cout << "    --size=<size>             The total file size per thread for the benchmark." << std::endl;
   std::cout << "                              (Default: 1G)" << std::endl;
   std::cout << "    --threads=<threadcount>   The number of client streams (files) per target to" << std::endl;
   std::cout << "                              be used in the benchmark. (Default: 1)" << std::endl;
   std::cout << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "    --verbose  Print result of every target." << std::endl;
   std::cout << "    --wait     Waits until the benchmark is finished." << std::endl;
   std::cout << "    --odirect  Use direct I/O (O_DIRECT) on the storage servers." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode runs a streaming benchmark on the storage targets without any extra" << std::endl;
   std::cout << " network communication for file data transfer. Thus, it allows to measure" << std::endl;
   std::cout << " storage performance independent of network performance." << std::endl;
   std::cout << std::endl;
   std::cout << " Note:" << std::endl;
   std::cout << "  Benchmark files will not be deleted automatically. Use --cleanup to delete the" << std::endl;
   std::cout << "  files after benchmarking." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Start a write benchmark on all storage targets of all servers with a" << std::endl;
   std::cout << "  blocksize of 512 KiB and a file size of 200 GiB, simulating 10 client streams" << std::endl;
   std::cout << "  # beegfs-ctl --storagebench --alltargets --write --blocksize=512K \\" << std::endl;
   std::cout << "     --size=200G --threads=10" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Query benchmark status/result of all targets" << std::endl;
   std::cout << "  # beegfs-ctl --storagebench --alltargets --status" << std::endl;
}

/*
 * send the commands to the storage nodes and collects the responses
 *
 * @param nodeID the nodeID of the destination storage server
 * @param response returns the content of the response message from the storage server
 * @return error information, true on success, false if a error occurred
 *
 */
bool ModeStorageBench::sendCmdAndCollectResult(NumNodeID nodeID,
   StorageBenchResponseInfo &response)
{
   bool retVal = false;
   NodeStoreServers* nodes = Program::getApp()->getStorageNodes();

   StorageBenchControlMsgResp* respMsgCast = NULL;

   UInt16List targets;
   this->cfgTargetIDs.getTargetsByNode(nodeID, targets);

   StorageBenchControlMsg msg(this->cfgAction, this->cfgType, this->cfgBlocksize, this->cfgSize,
      this->cfgThreads, this->cfgODirect, &targets);

   auto node = nodes->referenceNode(nodeID);
   if(!node)
   {
      std::cerr << "Unknown nodeID: " << nodeID << std::endl;
      return false;
   }

   const auto respMsg = MessagingTk::requestResponse(*node, msg,
         NETMSGTYPE_StorageBenchControlMsgResp);
   if (!respMsg)
   {
      std::cerr << "Failed to communicate with node: " << node->getTypedNodeID() << std::endl;
      retVal = false;
      goto err_cleanup;
   }

   respMsgCast = (StorageBenchControlMsgResp*)respMsg.get();
   respMsgCast->parseResults(&response.results);

   response.nodeID = nodeID;
   response.targetIDs = targets;
   response.errorCode = respMsgCast->getErrorCode();
   response.status = respMsgCast->getStatus();
   response.action = respMsgCast->getAction();
   response.type = respMsgCast->getType();

   retVal = true;

err_cleanup:
   return retVal;
}
