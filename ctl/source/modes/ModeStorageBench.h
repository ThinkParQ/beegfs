#ifndef MODESTORAGEBENCH_H_
#define MODESTORAGEBENCH_H_

#include <common/benchmark/StorageBench.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include "Mode.h"



//defines for the default values of the storage benchmark
#define STORAGEBENCH_DEFAULT_BLOCKSIZE                (1024 * 128)
#define STORAGEBENCH_DEFAULT_SIZE                     (1024 * 1024 * 1024)
#define STORAGEBENCH_DEFAULT_THREAD_COUNT             1


struct StorageBenchResponseInfo
{
   NumNodeID nodeID;
   UInt16List targetIDs;
   int errorCode;
   StorageBenchStatus status;
   StorageBenchAction action;
   StorageBenchType type;
   StorageBenchResultsMap results;
};

typedef std::list<StorageBenchResponseInfo> StorageBenchResponseInfoList;
typedef StorageBenchResponseInfoList::iterator StorageBenchResponseInfoListIter;
typedef StorageBenchResponseInfoList::const_iterator StorageBenchResponseInfoListConstIter;


class ModeStorageBench: public Mode
{
   public:
      ModeStorageBench()
      {
         this->cfgAllTargets = false;
         this->cfgStorageServer = StringList();
         this->cfgType = StorageBenchType_NONE;
         this->cfgBlocksize = STORAGEBENCH_DEFAULT_BLOCKSIZE;
         this->cfgSize = STORAGEBENCH_DEFAULT_SIZE;
         this->cfgAction = StorageBenchAction_NONE;
         this->cfgThreads = STORAGEBENCH_DEFAULT_THREAD_COUNT;
         this->cfgVerbose = false;
         this->cfgWait = false;
         this->cfgODirect = false;
      }

      virtual int execute();

      static void printHelp();

   protected:

   private:
      TargetMapper cfgTargetIDs;
      bool cfgAllTargets;
      StringList cfgStorageServer;

      StorageBenchAction cfgAction;
      StorageBenchType cfgType;
      bool cfgVerbose;
      bool cfgWait;
      bool cfgODirect;

      int64_t cfgBlocksize;
      int64_t cfgSize;
      int cfgThreads;

      int checkConfig(Node& mgmtNode, NodeStoreServers* storageNodes, StringMap* cfg);
      bool sendCmdAndCollectResult(NumNodeID nodeID, StorageBenchResponseInfo& response);

      void printResults(StorageBenchResponseInfoList* responses);
      void printShortResults(StorageBenchResponseInfoList* responses);
      void printVerboseResults(StorageBenchResponseInfoList* responses);
      void printError(StorageBenchResponseInfoList* responses);
      bool checkAndPrintCurrentStatus(StorageBenchResponseInfoList* responses,
         bool printStatus);
};

#endif /* MODESTORAGEBENCH_H_ */
