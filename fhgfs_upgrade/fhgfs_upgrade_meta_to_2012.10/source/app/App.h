#ifndef APP_H_
#define APP_H_

#include <app/config/Config.h>
#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/StatsCollector.h>
//#include <common/components/StreamListener.h>
#include <common/components/worker/Worker.h>
#include <common/nodes/TargetCapacityPools.h>
#include <common/nodes/TargetMapper.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <components/fullrefresher/FullRefresher.h>
#include <components/DatagramListener.h>
#include <components/HeartbeatManager.h>
#include <components/InternodeSyncer.h>
#include <net/message/NetMessageFactory.h>
#include <nodes/MetaNodeOpStats.h>
#include <nodes/NodeStoreEx.h>
#include <nodes/NodeStoreClientsEx.h>
#include <nodes/NodeStoreServersEx.h>
#include <storage/DirInode.h>
#include <storage/MetaStore.h>
#include <session/SessionStore.h>
#include <storage/SyncedDiskAccessPath.h>
//#include <testing/TestRunner.h>

#define FHGFS_VERSION "2012.10"
#define FHGFS_VERSION_CODE 1

#ifndef FHGFS_VERSION
   #error FHGFS_VERSION undefined
#endif

#if !defined(FHGFS_VERSION_CODE) || (FHGFS_VERSION_CODE == 0)
   #error FHGFS_VERSION_CODE undefined
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

class App : public AbstractApp
{
   public:
      App(int argc, char** argv);
      virtual ~App();

      virtual void run();

      void stopComponents();
      void handleComponentException(std::exception& e);

   private:
      int appResult;
      int argc;
      char** argv;

      Config*  cfg;
      Logger*  logger;
      LogContext* log;

      int pidFileLockFD; // -1 if unlocked, >=0 otherwise
      int workingDirLockFD; // -1 if unlocked, >=0 otherwise

      NetFilter* netFilter; // empty filter means "all nets allowed"
      StringList* allowedInterfaces; // empty list means "all interfaces accepted"
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      uint16_t localNodeNumID; // 0 means invalid/undefined
      std::string localNodeID;
      Node* localNode;

      NodeStoreServersEx* mgmtNodes;
      NodeStoreServersEx* metaNodes;
      NodeStoreServersEx* storageNodes;
      NodeStoreClientsEx* clientNodes;

      TargetCapacityPools* metaCapacityPools;
      TargetCapacityPools* storageCapacityPools;
      TargetMapper* targetMapper;

      MultiWorkQueue* workQueue;
      MultiWorkQueue* commSlaveQueue;
      NetMessageFactory* netMessageFactory;
      MetaStore* metaStore;

      DirInode* rootDir;
      DirInode* disposalDir;

      SessionStore* sessions;
      AcknowledgmentStore* ackStore;
      MetaNodeOpStats* clientOperationStats; // file system operation statistics per client

      std::string metaPathStr; // the general parent directory for all saved data

      Path* oldInodesPath; // for upgrade
      Path* inodesPath; // contains the actualy file/directory metadata
      Path* dentriesPath; // contains the file/directory structural links

      DatagramListener* dgramListener;
      HeartbeatManager* heartbeatMgr;
      // StreamListener* streamListener;
      StatsCollector* statsCollector;
      InternodeSyncer* internodeSyncer;
      FullRefresher* fullRefresher;
      WorkerList workerList;
      WorkerList commSlaveList; // used by workers for parallel comm tasks

      // TestRunner *testRunner;

      unsigned nextNumaBindTarget; // the numa node to which we will bind the next component thread

      StringUnsignedMap metaIdMap; // meta string to numeric ID map (upgrade tool)

      void runNormal();

      void runUnitTests();
      bool runStartupTests();
      bool runComponentTests();
      bool runIntegrationTests();

      void workersInit() throw(ComponentInitException);
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void commSlavesInit() throw(ComponentInitException);
      void commSlavesStart();
      void commSlavesStop();
      void commSlavesDelete();
      void commSlavesJoin();

      void initLogging() throw(InvalidConfigException);
      void initDataObjects() throw(InvalidConfigException);
      void initNet() throw(InvalidConfigException);
      void initLocalNodeIDs() throw(InvalidConfigException);
      void initLocalNode() throw(InvalidConfigException);
      void preinitStorage() throw(InvalidConfigException);
      void initStorage() throw(InvalidConfigException);
      void initRootDir() throw(InvalidConfigException);
      void initDisposalDir() throw(InvalidConfigException);
      void initComponents() throw(ComponentInitException);

      void startComponents();
      void joinComponents();

      bool waitForMgmtNode();
      bool preregisterNode();

      void logInfos();

      void daemonize() throw(InvalidConfigException);

      void registerSignalHandler();
      static void signalHandler(int sig);


   public:

      void initLocalNodeNumIDFile() throw(InvalidConfigException);

      // inliners

      /**
       * @return NULL for invalid node types
       */
      NodeStoreServersEx* getServerStoreFromType(NodeType nodeType) const
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
      AbstractNodeStore* getAbstractNodeStoreFromType(NodeType nodeType) const
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

      virtual Logger* getLogger()
      {
         return logger;
      }

      virtual ICommonConfig* getCommonConfig()
      {
         return cfg;
      }

      virtual NetFilter* getNetFilter()
      {
         return netFilter;
      }

      virtual AbstractNetMessageFactory* getNetMessageFactory()
      {
         return netMessageFactory;
      }

      AcknowledgmentStore* getAckStore() const
      {
         return ackStore;
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

      std::string getLocalNodeStrID(void) const
      {
         return this->localNodeID;
      }

      void setLocalNodeNumID(uint16_t newID)
      {
         localNodeNumID = newID;
      }

      Node* getLocalNode() const
      {
         return localNode;
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

      TargetCapacityPools* getMetaCapacityPools() const
      {
         return metaCapacityPools;
      }

      TargetCapacityPools* getStorageCapacityPools() const
      {
         return storageCapacityPools;
      }

      TargetMapper* getTargetMapper() const
      {
         return targetMapper;
      }

      MultiWorkQueue* getWorkQueue() const
      {
         return workQueue;
      }

      MultiWorkQueue* getCommSlaveQueue() const
      {
         return commSlaveQueue;
      }

      MetaStore* getMetaStore() const
      {
         return metaStore;
      }

      DirInode* getRootDir() const
      {
         return rootDir;
      }

      DirInode* getDisposalDir() const
      {
         return disposalDir;
      }

      SessionStore* getSessions() const
      {
         return sessions;
      }

      std::string getMetaPath() const
      {
         return metaPathStr;
      }

      MetaNodeOpStats* getClientOpStats() const
      {
         return clientOperationStats;
      }

      const Path* getEntriesPath() const
      {
         return inodesPath;
      }

      const Path* getOldInodesPath() const
      {
         return oldInodesPath;
      }

      const Path* getStructurePath() const
      {
         return dentriesPath;
      }

      HeartbeatManager* getHeartbeatMgr() const
      {
         return heartbeatMgr;
      }

      DatagramListener* getDatagramListener() const
      {
         return dgramListener;
      }

#if 0
      StreamListener* getStreamListener() const
      {
         return streamListener;
      }
#endif

      StatsCollector* getStatsCollector() const
      {
         return statsCollector;
      }

      InternodeSyncer* getInternodeSyncer() const
      {
         return internodeSyncer;
      }

      FullRefresher* getFullRefresher() const
      {
         return fullRefresher;
      }

      int getAppResult() const
      {
         return appResult;
      }

      StringUnsignedMap* getMetaStringIdMap()
      {
         return &this->metaIdMap;
      }

};


#endif /*APP_H_*/
