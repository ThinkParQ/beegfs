#ifndef APP_H_
#define APP_H_

#include <app/config/Config.h>
#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/StatsCollector.h>
#include <common/components/streamlistenerv2/ConnAcceptor.h>
#include <common/components/streamlistenerv2/StreamListenerV2.h>
#include <common/components/worker/Worker.h>
#include <common/components/TimerQueue.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeCapacityPools.h>
#include <common/nodes/TargetCapacityPools.h>
#include <common/nodes/TargetMapper.h>
#include <common/nodes/TargetStateStore.h>
#include <common/storage/quota/ExceededQuotaPerTarget.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <components/DatagramListener.h>
#include <components/InternodeSyncer.h>
#include <components/buddyresyncer/BuddyResyncer.h>
#include <net/message/NetMessageFactory.h>
#include <nodes/MetaNodeOpStats.h>
#include <nodes/NodeStoreEx.h>
#include <nodes/NodeStoreClientsEx.h>
#include <nodes/NodeStoreServersEx.h>
#include <session/SessionStore.h>
#include <storage/DirInode.h>
#include <storage/MetaStore.h>
#include <storage/SyncedDiskAccessPath.h>



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

typedef std::vector<StreamListenerV2*> StreamLisVec;
typedef StreamLisVec::iterator StreamLisVecIter;


// forward declarations
class LogContext;
class ModificationEventFlusher;
class FileEventLogger;


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
      LogContext* log;

      int pidFileLockFD; // -1 if unlocked, >=0 otherwise
      int workingDirLockFD; // -1 if unlocked, >=0 otherwise

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      std::shared_ptr<Node> localNode;

      NodeStoreServersEx* mgmtNodes;
      NodeStoreServersEx* metaNodes;
      NodeStoreServersEx* storageNodes;
      NodeStoreClientsEx* clientNodes;

      NodeCapacityPools* metaCapacityPools;
      NodeCapacityPools* metaBuddyCapacityPools;

      TargetMapper* targetMapper;
      MirrorBuddyGroupMapper* storageBuddyGroupMapper; // maps storage targets to buddy groups
      MirrorBuddyGroupMapper* metaBuddyGroupMapper; // maps meta nodes to buddy groups
      TargetStateStore* targetStateStore; // map storage targets to a state
      TargetStateStore* metaStateStore; // map mds targets (i.e. nodeIDs) to a state
      std::unique_ptr<StoragePoolStore> storagePoolStore; // stores (category) storage pools

      MultiWorkQueue* workQueue;
      MultiWorkQueue* commSlaveQueue;
      NetMessageFactory* netMessageFactory;
      MetaStore* metaStore;

      DirInode* rootDir;
      bool isRootBuddyMirrored;
      DirInode* disposalDir;
      DirInode* buddyMirrorDisposalDir;

      SessionStore* sessions;
      SessionStore* mirroredSessions;
      AcknowledgmentStore* ackStore;
      MetaNodeOpStats* nodeOperationStats; // file system operation statistics

      std::string metaPathStr; // the general parent directory for all saved data
      Path* inodesPath; // contains the actualy file/directory metadata
      Path* dentriesPath; // contains the file/directory structural links
      Path* buddyMirrorInodesPath; // contains the inodes for buddy mirrored inodes
      Path* buddyMirrorDentriesPath; // contains the dentries for buddy mirrored dentries

      DatagramListener* dgramListener;
      ConnAcceptor* connAcceptor;
      StatsCollector* statsCollector;
      InternodeSyncer* internodeSyncer;
      ModificationEventFlusher* modificationEventFlusher;
      TimerQueue* timerQueue;

      unsigned numStreamListeners; // value copied from cfg (for performance)
      StreamLisVec streamLisVec;

      WorkerList workerList;
      WorkerList commSlaveList; // used by workers for parallel comm tasks

      BuddyResyncer* buddyResyncer;

      ExceededQuotaPerTarget exceededQuotaStores;

      std::unique_ptr<FileEventLogger> fileEventLogger;

      unsigned nextNumaBindTarget; // the numa node to which we will bind the next component thread

      void runNormal();

      void streamListenersInit() throw(ComponentInitException);
      void streamListenersStart();
      void streamListenersStop();
      void streamListenersDelete();
      void streamListenersJoin();

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
      void initBasicNetwork();
      void initLocalNodeIDs(std::string& outLocalID, NumNodeID& outLocalNumID);
      void initLocalNode(std::string& localNodeID, NumNodeID localNodeNumID);
      void initLocalNodeNumIDFile(NumNodeID localNodeNumID);
      bool preinitStorage() throw(InvalidConfigException);
      void initStorage() throw(InvalidConfigException);
      void initXAttrLimit();
      void initRootDir(NumNodeID localNodeNumID);
      void initDisposalDir() throw(InvalidConfigException);
      void initComponents(TargetConsistencyState initialConsistencyState)
         throw(ComponentInitException);

      void startComponents();
      void joinComponents();

      bool waitForMgmtNode();
      bool preregisterNode(std::string& localNodeID, NumNodeID& outLocalNodeNumID);
      bool downloadMgmtInfo(TargetConsistencyState& outInitialConsistencyState);

      void logInfos();

      void daemonize() throw(InvalidConfigException);

      void registerSignalHandler();
      static void signalHandler(int sig);

      bool restoreSessions();
      bool storeSessions();
      bool deleteSessionFiles();


   public:
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

      /**
       * Get one of the available stream listeners based on the socket file descriptor number.
       * This is to load-balance the sockets over all available stream listeners and ensure that
       * sockets are not bouncing between different stream listeners.
       *
       * Note that IB connections eat two fd numbers, so 2 and multiples of 2 might not be a good
       * value for number of stream listeners.
       */
      virtual StreamListenerV2* getStreamListenerByFD(int fd)
      {
         return streamLisVec[fd % numStreamListeners];
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

      /*
       * this is just a convenience wrapper for now; old code used to have the localNodeNumID as a
       * member of App, but localNodeNumID and the numID in localNode are duplicates
       */
      NumNodeID getLocalNodeNumID() const
      {
         return localNode->getNumID();
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

      TargetStateStore* getTargetStateStore() const
      {
         return targetStateStore;
      }

      TargetStateStore* getMetaStateStore() const
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

      DirInode* getBuddyMirrorDisposalDir() const
      {
         return buddyMirrorDisposalDir;
      }

      SessionStore* getSessions() const
      {
         return sessions;
      }

      SessionStore* getMirroredSessions() const
      {
         return mirroredSessions;
      }

      std::string getMetaPath() const
      {
         return metaPathStr;
      }

      MetaNodeOpStats* getNodeOpStats() const
      {
         return nodeOperationStats;
      }

      const Path* getInodesPath() const
      {
         return inodesPath;
      }

      const Path* getDentriesPath() const
      {
         return dentriesPath;
      }

      const Path* getBuddyMirrorInodesPath() const
      {
         return buddyMirrorInodesPath;
      }

      const Path* getBuddyMirrorDentriesPath() const
      {
         return buddyMirrorDentriesPath;
      }

      DatagramListener* getDatagramListener() const
      {
         return dgramListener;
      }

      const StreamLisVec* getStreamListenerVec() const
      {
         return &streamLisVec;
      }

      StatsCollector* getStatsCollector() const
      {
         return statsCollector;
      }

      InternodeSyncer* getInternodeSyncer() const
      {
         return internodeSyncer;
      }

      TimerQueue* getTimerQueue() const
      {
         return timerQueue;
      }

      ModificationEventFlusher* getModificationEventFlusher() const
      {
         return modificationEventFlusher;
      }

      WorkerList* getWorkers()
      {
         return &workerList;
      }

      BuddyResyncer* getBuddyResyncer()
      {
         return this->buddyResyncer;
      }

      int getAppResult() const
      {
         return appResult;
      }

      const ExceededQuotaPerTarget* getExceededQuotaStores() const
      {
         return &exceededQuotaStores;
      }
      
      StoragePoolStore* getStoragePoolStore() const
      {
         return storagePoolStore.get();
      }

      FileEventLogger* getFileEventLogger()
      {
         return fileEventLogger.get();
      }
};


#endif /*APP_H_*/
