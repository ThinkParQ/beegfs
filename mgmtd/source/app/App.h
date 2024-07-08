#ifndef APP_H_
#define APP_H_

#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/Worker.h>
#include <common/components/StatsCollector.h>
#include <common/components/StreamListener.h>
#include <common/nodes/NodeCapacityPools.h>
#include <common/nodes/RootInfo.h>
#include <common/nodes/TargetCapacityPools.h>
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
#include <nodes/MirrorBuddyGroupMapperEx.h>
#include <nodes/MgmtdTargetStateStore.h>
#include <nodes/StoragePoolStoreEx.h>
#include <nodes/TargetMapperEx.h>
#include "config/Config.h"

#ifndef BEEGFS_VERSION
   #error BEEGFS_VERSION undefined
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

      virtual void run() override;

      void shutDown(bool clean);
      virtual void handleComponentException(std::exception& e) override;
      virtual void handleNetworkInterfaceFailure(const std::string& devname) override;

   private:
      int appResult;
      int argc;
      char** argv;

      Config*  cfg;
      LogContext* log;
      std::list<std::string> allowedInterfaces;

      LockFD pidFileLockFD;
      LockFD workingDirLockFD;

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      uint16_t localNodeNumID; // 0 means invalid/undefined
      std::string localNodeID;
      std::shared_ptr<Node> localNode;

      AtomicSizeT shuttingDown; // AppShutdownState

      NodeStoreServersEx* mgmtNodes;
      NodeStoreServersEx* metaNodes;
      NodeStoreServersEx* storageNodes;
      NodeStoreClientsEx* clientNodes;

      RootInfo metaRoot;

      NodeCapacityPools* metaCapacityPools;
      NodeCapacityPools* metaBuddyCapacityPools;
      TargetMapperEx* targetMapper; // maps targetNumIDs to nodes
      NumericIDMapper* targetNumIDMapper; // maps targetStringIDs to targetNumIDs
      MirrorBuddyGroupMapperEx* storageBuddyGroupMapper; // maps storage targets to MBGs
      MirrorBuddyGroupMapperEx* metaBuddyGroupMapper; // maps metadata nodes to MBGs
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

      virtual void stopComponents() override;

      void workersInit();
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initDataObjects(int argc, char** argv);
      void initLocalNodeInfo();
      bool preinitStorage();
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

      bool checkEnterpriseFeatureUsage();

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

      virtual const ICommonConfig* getCommonConfig() const override
      {
         return cfg;
      }

      virtual const NetFilter* getNetFilter() const override
      {
         return netFilter;
      }

      virtual const NetFilter* getTcpOnlyFilter() const override
      {
         return tcpOnlyFilter;
      }

      virtual const AbstractNetMessageFactory* getNetMessageFactory() const override
      {
         return netMessageFactory;
      }

      Config* getConfig() const
      {
         return cfg;
      }

      uint16_t getLocalNodeNumID() const
      {
         return localNodeNumID;
      }

      void updateLocalNicList(NicAddressList& localNicList);

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

      TargetMapperEx* getTargetMapper() const
      {
         return targetMapper;
      }

      MirrorBuddyGroupMapperEx* getStorageBuddyGroupMapper() const
      {
         return storageBuddyGroupMapper;
      }

      MirrorBuddyGroupMapperEx* getMetaBuddyGroupMapper() const
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

      const RootInfo& getMetaRoot() const { return metaRoot; }
      RootInfo& getMetaRoot() { return metaRoot; }
      void findAllowedInterfaces(NicAddressList& outList) const;
};


#endif /*APP_H_*/
