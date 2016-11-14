#ifndef APP_H_
#define APP_H_

#include <app/config/Config.h>
#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/Worker.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeStoreClients.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/nodes/TargetMapper.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <components/DatagramListener.h>
#include <components/HeartbeatManager.h>
#include <net/message/NetMessageFactory.h>
#include <testing/TestRunner.h>


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
#define APPCODE_INVALID_RESULT         4


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

      Config* cfg;
      Logger* logger;
      LogContext* log;

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      StringList* allowedInterfaces; // empty list means "all interfaces accepted"
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      std::shared_ptr<Node> localNode;

      NodeStoreServers* mgmtNodes;
      NodeStoreServers* metaNodes;
      NodeStoreServers* storageNodes;
      NodeStoreClients* clientNodes;

      TargetMapper* targetMapper;
      MirrorBuddyGroupMapper* mirrorBuddyGroupMapper;
      MirrorBuddyGroupMapper* metaMirrorBuddyGroupMapper;
      TargetStateStore* targetStateStore;
      TargetStateStore* metaTargetStateStore;

      MultiWorkQueue* workQueue;
      AcknowledgmentStore* ackStore;
      NetMessageFactory* netMessageFactory;

      DatagramListener* dgramListener;
      HeartbeatManager* heartbeatMgr;
      WorkerList workerList;

      TestRunner *testRunner;

      void runNormal();
      void runUnitTests();

      int executeMode(const RunModesElem* runMode);

      void workersInit() throw(ComponentInitException);
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initDataObjects() throw(InvalidConfigException);
      void initLocalNodeInfo() throw(InvalidConfigException);
      void initComponents() throw(ComponentInitException);
      void startComponents();
      void joinComponents();

      void logInfos();

      void daemonize() throw(InvalidConfigException);

      void registerSignalHandler();
      static void signalHandler(int sig);


   public:
      // inliners

      /**
       * @return NULL for invalid node types
       */
      NodeStoreServers* getServerStoreFromType(NodeType nodeType)
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

      Config* getConfig() const
      {
         return cfg;
      }

      Node& getLocalNode() const
      {
         return *localNode;
      }

      NodeStoreServers* getMgmtNodes() const
      {
         return mgmtNodes;
      }

      NodeStoreServers* getMetaNodes() const
      {
         return metaNodes;
      }

      NodeStoreServers* getStorageNodes() const
      {
         return storageNodes;
      }

      NodeStoreClients* getClientNodes() const
      {
         return clientNodes;
      }

      TargetMapper* getTargetMapper() const
      {
         return targetMapper;
      }

      MirrorBuddyGroupMapper* getMirrorBuddyGroupMapper() const
      {
         return mirrorBuddyGroupMapper;
      }

      MirrorBuddyGroupMapper* getMetaMirrorBuddyGroupMapper() const
      {
         return metaMirrorBuddyGroupMapper;
      }

      TargetStateStore* getTargetStateStore() const
      {
         return targetStateStore;
      }

      TargetStateStore* getMetaTargetStateStore() const
      {
         return metaTargetStateStore;
      }

      MultiWorkQueue* getWorkQueue() const
      {
         return workQueue;
      }

      AcknowledgmentStore* getAckStore() const
      {
         return ackStore;
      }

      HeartbeatManager* getHeartbeatMgr() const
      {
         return heartbeatMgr;
      }

      DatagramListener* getDatagramListener() const
      {
         return dgramListener;
      }

      int getAppResult() const
      {
         return appResult;
      }

};


#endif /*APP_H_*/
