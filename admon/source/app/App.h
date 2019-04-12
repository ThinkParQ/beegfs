#ifndef APP_H_
#define APP_H_

#include <app/config/Config.h>
#include <app/config/RuntimeConfig.h>
#include <common/app/AbstractApp.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/Common.h>
#include <common/components/ComponentInitException.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/worker/Worker.h>
#include <common/nodes/LocalNode.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeStoreClients.h>
#include <common/nodes/Node.h>
#include <common/nodes/RootInfo.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NetFilter.h>
#include <components/Database.h>
#include <components/DatagramListener.h>
#include <components/ExternalJobRunner.h>
#include <components/InternodeSyncer.h>
#include <components/Mailer.h>
#include <components/requestor/DataRequestorIOStats.h>
#include <components/requestor/NodeListRequestor.h>
#include <components/StatsOperator.h>
#include <components/WebServer.h>
#include <net/message/NetMessageFactory.h>
#include <nodes/NodeStoreMetaEx.h>
#include <nodes/NodeStoreStorageEx.h>
#include <nodes/NodeStoreMgmtEx.h>

#include <signal.h>



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

class App : public AbstractApp
{
   public:
      App(int argc, char** argv);
      virtual ~App();

      virtual void run() override;

      virtual void stopComponents() override;
      virtual void handleComponentException(std::exception& e) override;

   private:
      int appResult;
      int argc;
      char** argv;

      Config* cfg;
      RuntimeConfig *runtimeCfg;
      LogContext* log;
      std::list<std::string> allowedInterfaces;

      LockFD pidFileLockFD;

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      std::shared_ptr<Node> localNode;

      MultiWorkQueue* workQueue;
      AcknowledgmentStore* ackStore;
      NetMessageFactory* netMessageFactory;
      InternodeSyncer* internodeSyncer;

      DatagramListener* datagramListener;
      WebServer* webServer;
      WorkerList workerList;

      NodeListRequestor *nodeListRequestor;

      NodeStoreStorageEx* storageNodes;
      NodeStoreMetaEx* metaNodes;
      NodeStoreMgmtEx* mgmtNodes;
      NodeStoreClients* clientNodes;

      RootInfo metaRoot;

      TargetMapper* targetMapper;

      MirrorBuddyGroupMapper* metaBuddyGroupMapper;
      MirrorBuddyGroupMapper* storageBuddyGroupMapper;

      StatsOperator* clientStatsOperator;
      DataRequestorIOStats* dataRequestorIOStats;
      Database* db;

      Mailer* mailer;
      ExternalJobRunner* jobRunner;

      void runNormal();

      void workersInit();
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initDataObjects(int argc, char** argv);
      void initLocalNodeInfo();
      void initComponents();
      void startComponents();
      void joinComponents();

      void logInfos();

      void registerSignalHandler();
      static void signalHandler(int sig);
      void daemonize();

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

      void getStorageNodesAsStringList(StringList *outList);
      void getMetaNodesAsStringList(StringList *outList);
      void getStorageNodeNumIDs(NumNodeIDList *outList);
      void getMetaNodeNumIDs(NumNodeIDList *outList);

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

      AcknowledgmentStore* getAckStore() const
      {
         return ackStore;
      }

      Config* getConfig()
      {
         return cfg;
      }

      RuntimeConfig* getRuntimeConfig()
      {
         return runtimeCfg;
      }

      Node& getLocalNode()
      {
         return *localNode;
      }

      DatagramListener* getdatagramListener()
      {
         return datagramListener;
      }

      int getAppResult()
      {
         return appResult;
      }

      Mailer *getMailer()
      {
         return mailer;
      }

      ExternalJobRunner *getJobRunner()
      {
         return jobRunner;
      }

      NodeStoreStorageEx *getStorageNodes()
      {
         return storageNodes;
      }

      NodeStoreMetaEx *getMetaNodes()
      {
         return metaNodes;
      }

      NodeStoreMgmtEx *getMgmtNodes()
      {
         return mgmtNodes;
      }

      NodeStoreClients *getClientNodes()
      {
         return clientNodes;
      }

      Database *getDB()
      {
         return db;
      }

      DatagramListener *getDatagramListener()
      {
         return datagramListener;
      }

      MultiWorkQueue *getWorkQueue()
      {
         return workQueue;
      }

      StatsOperator *getClientStatsOperator()
      {
         return clientStatsOperator;
      }

      InternodeSyncer* getInternodeSyncer() const
      {
         return internodeSyncer;
      }

      TargetMapper* getTargetMapper() const
      {
         return targetMapper;
      }

      MirrorBuddyGroupMapper* getMetaBuddyGroupMapper() const
      {
         return metaBuddyGroupMapper;
      }

      MirrorBuddyGroupMapper* getStorageBuddyGroupMapper() const
      {
         return storageBuddyGroupMapper;
      }

      const RootInfo& getMetaRoot() const { return metaRoot; }
      RootInfo& getMetaRoot() { return metaRoot; }
};


#endif /*APP_H_*/
