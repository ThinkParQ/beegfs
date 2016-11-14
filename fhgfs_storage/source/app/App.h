#ifndef APP_H_
#define APP_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/streamlistenerv2/ConnAcceptor.h>
#include <common/components/streamlistenerv2/StreamListenerV2.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/components/worker/Worker.h>
#include <common/components/TimerQueue.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/TargetStateStore.h>
#include <common/storage/Path.h>
#include <common/storage/Storagedata.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/storage/quota/ExceededQuotaStore.h>
#include <common/toolkit/NetFilter.h>
#include <common/Common.h>
#include <components/benchmarker/StorageBenchOperator.h>
#include <components/buddyresyncer/BuddyResyncer.h>
#include <components/chunkfetcher/ChunkFetcher.h>
#include <components/DatagramListener.h>
#include <components/InternodeSyncer.h>
#include <components/StorageStatsCollector.h>
#include <net/message/NetMessageFactory.h>
#include <nodes/NodeStoreEx.h>
#include <nodes/NodeStoreServersEx.h>
#include <nodes/StorageNodeOpStats.h>
#include <session/SessionStore.h>
#include <storage/ChunkLockStore.h>
#include <storage/ChunkStore.h>
#include <storage/SyncedStoragePaths.h>
#include <storage/StorageTargets.h>
#include <testing/TestRunner.h>
#include <toolkit/QuotaTk.h>


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

      Config* cfg;
      Logger* logger;
      LogContext* log;

      int pidFileLockFD; // -1 if unlocked, >=0 otherwise

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      StringList* allowedInterfaces; // empty list means "all interfaces accepted"
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      std::shared_ptr<Node> localNode;

      NodeStoreServersEx* mgmtNodes;
      NodeStoreServersEx* metaNodes; // needed for backward communication introduced with GAM integration
      NodeStoreServersEx* storageNodes;

      TargetMapper* targetMapper;
      MirrorBuddyGroupMapper* mirrorBuddyGroupMapper; // maps targets to mirrorBuddyGroups
      TargetStateStore* targetStateStore; // map storage targets to a state

      MultiWorkQueueMap workQueueMap; // maps targetIDs to WorkQueues
      SessionStore* sessions;
      StorageNodeOpStats* nodeOperationStats; // file system operation statistics
      AcknowledgmentStore* ackStore;
      NetMessageFactory* netMessageFactory;

      StorageTargets* storageTargets; // target IDs and corresponding storage paths
      SyncedStoragePaths* syncedStoragePaths; // serializes access to paths (=> entryIDs)
      StorageBenchOperator* storageBenchOperator; // benchmark for the storage

      DatagramListener* dgramListener;
      ConnAcceptor* connAcceptor;
      StatsCollector* statsCollector;
      InternodeSyncer* internodeSyncer;
      TimerQueue* timerQueue;

      ChunkFetcher* chunkFetcher;

      unsigned numStreamListeners; // value copied from cfg (for performance)
      StreamLisVec streamLisVec;

      WorkerList workerList;
      bool workersRunning;
      Mutex mutexWorkersRunning;

      TestRunner *testRunner;

      ChunkStore* chunkDirStore;

      unsigned nextNumaBindTarget; // the numa node to which we will bind the next component thread

      ExceededQuotaStore* exceededQuotaStore; // contains the UIDs and GIDs with exceeded quota

      BuddyResyncer* buddyResyncer;
      ChunkLockStore* chunkLockStore;

      void* dlOpenHandleLibZfs;  // handle of the libzfs from dlopen
      bool libZfsErrorReported;

      void runNormal();

      void runUnitTests();
      bool runStartupTests();
      bool runComponentTests();
      bool runIntegrationTests();

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

      void initLogging() throw(InvalidConfigException);
      void initDataObjects() throw(InvalidConfigException);
      void initBasicNetwork();
      void initLocalNodeIDs(std::string& outLocalNodeID, NumNodeID& outLocalNodeNumID);
      void initLocalNode(std::string& localNodeID, NumNodeID localNodeNumID);
      void initLocalNodeNumIDFile(NumNodeID localNodeNumID) ;
      void preinitStorage() throw(InvalidConfigException);
      void initStorage() throw(InvalidConfigException);
      void initPostTargetRegistration() throw(InvalidConfigException);
      void initComponents() throw(ComponentInitException);

      void startComponents();
      void joinComponents();

      bool waitForMgmtNode();
      bool preregisterNode(std::string& localNodeID, NumNodeID& outLocalNodeNumID);
      bool preregisterTargets(const NumNodeID localNodeNumID);
      bool preregisterTarget(Node& mgmtNode, std::string targetID, uint16_t targetNumID,
         uint16_t* outNewTargetNumID);
      bool registerAndDownloadMgmtInfo();

      void logInfos();

      void setUmask();

      void daemonize() throw(InvalidConfigException);

      void registerSignalHandler();
      static void signalHandler(int sig);

      bool restoreSessions();
      bool storeSessions();
      bool deleteSessionFiles();

      bool openLibZfs();
      bool closeLibZfs();


   public:
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

      Node& getLocalNode() const
      {
         return *localNode;
      }

      NodeStoreServersEx* getMgmtNodes() const
      {
         return mgmtNodes;
      }

      NodeStoreEx* getMetaNodes() const
      {
         return metaNodes;
      }

      NodeStoreServersEx* getStorageNodes() const
      {
         return storageNodes;
      }

      TargetMapper* getTargetMapper() const
      {
         return targetMapper;
      }

      MirrorBuddyGroupMapper* getMirrorBuddyGroupMapper() const
      {
         return mirrorBuddyGroupMapper;
      }

      TargetStateStore* getTargetStateStore() const
      {
         return targetStateStore;
      }

      MultiWorkQueue* getWorkQueue(uint16_t targetID) const
      {
         MultiWorkQueueMapCIter iter = workQueueMap.find(targetID);

         if(iter != workQueueMap.end() )
            return iter->second;

         /* note: it's not unusual to not find given targetID, e.g.
               - when per-target queues are disabled
               - or when server restarted without one of its targets (and clients don't know that)
               - or if client couldn't provide targetID because it's not a target message */

         return workQueueMap.begin()->second;
      }

      MultiWorkQueueMap* getWorkQueueMap()
      {
         return &workQueueMap;
      }

      SessionStore* getSessions() const
      {
         return sessions;
      }

      StorageNodeOpStats* getNodeOpStats() const
      {
         return nodeOperationStats;
      }

      StorageTargets* getStorageTargets() const
      {
         return storageTargets;
      }

      SyncedStoragePaths* getSyncedStoragePaths() const
      {
         return syncedStoragePaths;
      }

      StorageBenchOperator* getStorageBenchOperator() const
      {
         return this->storageBenchOperator;
      }

      /**
       * @param isMirrorFD true to get the fd for the mirror subdir, false to get the fd for the
       *    general chunks subdir.
       * @return -1 if no such target exists, otherwise the file descriptor to target/chunks dir or
       *    target/mirror dir.
       */
      int getTargetFD(const uint16_t targetID, const bool isMirrorFD) const
      {
         if (isMirrorFD)
            return storageTargets->getMirrorFD(targetID);
         else
            return storageTargets->getChunkFD(targetID);
      }

      /**
       * @param isMirrorFD true to get the fd for the mirror subdir, false to get the fd for the
       *    general chunks subdir.
       * @param outConsistencyState only valid if return is not -1.
       * @return -1 if no such target exists, otherwise the file descriptor to target/chunks dir or
       *    target/mirror dir.
       */
      int getTargetFDAndConsistencyState(const uint16_t targetID, const bool isMirrorFD,
         TargetConsistencyState* outConsistencyState) const
      {
            return storageTargets->getChunkFDAndConsistencyState(
               targetID, isMirrorFD, outConsistencyState);
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

      int getAppResult() const
      {
         return appResult;
      }

      bool getWorkersRunning()
      {
         bool retVal;

         SafeMutexLock lock(&this->mutexWorkersRunning); // L O C K
         retVal = this->workersRunning;
         lock.unlock(); // U N L O C K

         return retVal;
      }

      ChunkStore* getChunkDirStore() const
      {
         return this->chunkDirStore;
      }

      ChunkFetcher* getChunkFetcher() const
      {
         return this->chunkFetcher;
      }

      ExceededQuotaStore* getExceededQuotaStore() const
      {
         return this->exceededQuotaStore;
      }

      BuddyResyncer* getBuddyResyncer() const
      {
         return this->buddyResyncer;
      }

      ChunkLockStore* getChunkLockStore() const
      {
         return chunkLockStore;
      }

      WorkerList* getWorkers()
      {
         return &workerList;
      }

      void setLibZfsErrorReported(bool isReported)
      {
         libZfsErrorReported = isReported;
      }

      void* getDlOpenHandleLibZfs()
      {
         if(dlOpenHandleLibZfs)
            return dlOpenHandleLibZfs;
         else
         if(cfg->getQuotaDisableZfsSupport() )
         {
            if(!libZfsErrorReported)
            {
               logger->logErr("ZfsSupport", "Quota support for ZFS is disabled.");
               libZfsErrorReported = true;
            }
         }
         else
         if(!libZfsErrorReported)
            openLibZfs();

         return dlOpenHandleLibZfs;
      }

      bool isDlOpenHandleLibZfsValid()
      {
         if(dlOpenHandleLibZfs)
            return true;

         return false;
      }
};
#endif /*APP_H_*/
