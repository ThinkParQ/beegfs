#ifndef APP_H_
#define APP_H_

#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/Worker.h>
#include <common/components/StatsCollector.h>
#include <common/components/StreamListener.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeCapacityPools.h>
#include <common/nodes/TargetCapacityPools.h>
#include <common/nodes/TargetMapper.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <common/threading/Atomics.h>
#include <common/Common.h>
#include <components/quota/QuotaManager.h>
#include <components/DatagramListener.h>
#include <components/HeartbeatManager.h>
#include <components/InternodeSyncer.h>
#include <net/message/NetMessageFactory.h>
#include <nodes/NodeStoreClientsEx.h>
#include <nodes/NodeStoreServersEx.h>
#include <nodes/NumericIDMapper.h>
#include <nodes/MgmtdTargetStateStore.h>
#include <nodes/StoragePoolStoreEx.h>
#include "config/Config.h"

#ifndef BEEGFS_VERSION
   #error BEEGFS_VERSION undefined
#endif

#if !defined(BEEGFS_VERSION_CODE) || (BEEGFS_VERSION_CODE == 0)
   #error BEEGFS_VERSION_CODE undefined
#endif


// program return codes
#define APPCODE_NO_ERROR               0
#define APPCODE_INVALID_CONFIG         1
#define APPCODE_INITIALIZATION_ERROR   2
#define APPCODE_RUNTIME_ERROR          3


typedef std::list<Worker*> WorkerList;
typedef WorkerList::iterator WorkerListIter;


// forward declarations
class LogContext;

enum AppShutdownState {
   App_RUNNING,            // App is running normally
   App_SHUTDOWN_CLEAN,     // Shutdown has been requested (e.g. SIGINT)
   App_SHUTDOWN_IMMEDIATE  // Forceful shutdown (e.g. component exception)
};


class App : public AbstractApp
{
   public:
      App(int argc, char** argv);
      virtual ~App();

      virtual void run();

      void shutDown(bool clean);
      void handleComponentException(std::exception& e);

   private:
      int appResult;
      int argc;
      char** argv;

      Config*  cfg;
      LogContext* log;

      int pidFileLockFD; // -1 if unlocked, >=0 otherwise
      int workingDirLockFD; // -1 if unlocked, >=0 otherwise

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      uint16_t localNodeNumID; // 0 means invalid/undefined
      std::string localNodeID;
      std::shared_ptr<Node> localNode;

      AtomicSizeT shuttingDown; // AppShutdownState

      NodeStoreServersEx* mgmtNodes;
      NodeStoreServersEx* metaNodes;
      NodeStoreServersEx* storageNodes;
      NodeStoreClientsEx* clientNodes;

      NodeCapacityPools* metaCapacityPools;
      NodeCapacityPools* metaBuddyCapacityPools;
      TargetMapper* targetMapper; // maps targetNumIDs to nodes
      NumericIDMapper* targetNumIDMapper; // maps targetStringIDs to targetNumIDs
      MirrorBuddyGroupMapper* storageBuddyGroupMapper; // maps storage targets to MBGs
      MirrorBuddyGroupMapper* metaBuddyGroupMapper; // maps metadata nodes to MBGs
      MgmtdTargetStateStore* targetStateStore; // map storage targets to a state
      MgmtdTargetStateStore* metaStateStore; // map mds targets (i.e. nodeIDs) to a state
      std::unique_ptr<StoragePoolStoreEx> storagePoolStore; // stores (category) storage pools

      MultiWorkQueue* workQueue;
      AcknowledgmentStore* ackStore;
      NetMessageFactory* netMessageFactory;

      Path* mgmtdPath; // path to storage directory

      DatagramListener* dgramListener;
      HeartbeatManager* heartbeatMgr;
      StreamListener* streamListener;
      StatsCollector* statsCollector;
      InternodeSyncer* internodeSyncer;
      WorkerList workerList;

      QuotaManager* quotaManager;

      void runNormal();

      void stopComponents();

      void workersInit();
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initDataObjects(int argc, char** argv);
      void initLocalNodeInfo();
      bool preinitStorage();
      template<typename StoreT> void loadStoreFromFile(int format, LogContext& log, StoreT& store,
            char storeFlag, const std::string& path, const std::string& description);
      void initStorage(const bool firstRun);
      void readTargetStates(const bool firstRun, StringMap& formatProperties,
            MgmtdTargetStateStore* stateStore);
      void initRootDir();
      void initDisposalDir();
      void initComponents();
      void startComponents();
      void joinComponents();

      void logInfos();

      void daemonize();

      void registerSignalHandler();
      static void signalHandler(int sig);

      void notifyWorkers();
      void setPrimariesPOffline();

   public:
      // inliners

      /**
       * @return NULL for invalid node types
       */
      NodeStoreServersEx* getServerStoreFromType(NodeType nodeType)
      {
         switch(nodeType)
         {
            case NODETYPE_Meta:
               return metaNodes;

            case NODETYPE_Storage:
               return storageNodes;

            case NODETYPE_Mgmt:
               return mgmtNodes;

            default:
               return NULL;
         }
      }

      /**
       * @return NULL for invalid node types
       */
      AbstractNodeStore* getAbstractNodeStoreFromType(NodeType nodeType)
      {
         switch(nodeType)
         {
            case NODETYPE_Meta:
               return metaNodes;

            case NODETYPE_Storage:
               return storageNodes;

            case NODETYPE_Client:
               return clientNodes;

            case NODETYPE_Mgmt:
               return mgmtNodes;

            default:
               return NULL;
         }
      }


      // getters & setters

      virtual ICommonConfig* getCommonConfig()
      {
         return cfg;
      }

      virtual NetFilter* getNetFilter()
      {
         return netFilter;
      }

      virtual NetFilter* getTcpOnlyFilter()
      {
         return tcpOnlyFilter;
      }

      virtual AbstractNetMessageFactory* getNetMessageFactory()
      {
         return netMessageFactory;
      }

      Config* getConfig() const
      {
         return cfg;
      }

      NicAddressList getLocalNicList() const
      {
         return localNicList;
      }

      uint16_t getLocalNodeNumID() const
      {
         return localNodeNumID;
      }

      Node& getLocalNode() const
      {
         return *localNode;
      }

      NodeStoreServersEx* getMgmtNodes() const
      {
         return mgmtNodes;
      }

      NodeStoreServersEx* getMetaNodes() const
      {
         return metaNodes;
      }

      NodeStoreServersEx* getStorageNodes() const
      {
         return storageNodes;
      }

      NodeStoreClientsEx* getClientNodes() const
      {
         return clientNodes;
      }

      NodeCapacityPools* getMetaCapacityPools() const
      {
         return metaCapacityPools;
      }

      TargetMapper* getTargetMapper() const
      {
         return targetMapper;
      }

      MirrorBuddyGroupMapper* getStorageBuddyGroupMapper() const
      {
         return storageBuddyGroupMapper;
      }

      MirrorBuddyGroupMapper* getMetaBuddyGroupMapper() const
      {
         return metaBuddyGroupMapper;
      }

      NumericIDMapper* getTargetNumIDMapper() const
      {
         return targetNumIDMapper;
      }

      MgmtdTargetStateStore* getTargetStateStore() const
      {
         return targetStateStore;
      }

      MgmtdTargetStateStore* getMetaStateStore() const
      {
         return metaStateStore;
      }

      NodeCapacityPools* getMetaBuddyCapacityPools() const
      {
         return metaBuddyCapacityPools;
      }

      MultiWorkQueue* getWorkQueue() const
      {
         return workQueue;
      }

      AcknowledgmentStore* getAckStore() const
      {
         return ackStore;
      }

      Path* getMgmtdPath() const
      {
         return mgmtdPath;
      }

      HeartbeatManager* getHeartbeatMgr() const
      {
         return heartbeatMgr;
      }

      DatagramListener* getDatagramListener() const
      {
         return dgramListener;
      }

      StreamListener* getStreamListener() const
      {
         return streamListener;
      }

      StatsCollector* getStatsCollector() const
      {
         return statsCollector;
      }

      InternodeSyncer* getInternodeSyncer() const
      {
         return internodeSyncer;
      }

      int getAppResult() const
      {
         return appResult;
      }

      QuotaManager* getQuotaManager()
      {
         return quotaManager;
      }

      StoragePoolStoreEx* getStoragePoolStore() const
      {
         return storagePoolStore.get();
      }

      bool isShuttingDown() const
      {
         return shuttingDown.read() != App_RUNNING;
      }
};


#endif /*APP_H_*/
