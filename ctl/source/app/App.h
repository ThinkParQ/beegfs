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
#include <common/nodes/RootInfo.h>
#include <common/nodes/StoragePoolStore.h>
#include <common/nodes/TargetMapper.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <components/DatagramListener.h>
#include <net/message/NetMessageFactory.h>


#ifndef BEEGFS_VERSION
   #error BEEGFS_VERSION undefined
#endif

// program return codes
#define APPCODE_NO_ERROR               0
#define APPCODE_INVALID_CONFIG         1
#define APPCODE_INITIALIZATION_ERROR   2
#define APPCODE_RUNTIME_ERROR          3
#define APPCODE_INVALID_RESULT         4
#define APPCODE_NODE_NOT_REACHABLE     5
#define APPCODE_TARGET_NOT_ONLINE      6
#define APPCODE_TARGET_NOT_GOOD        7


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
      virtual void handleNetworkInterfaceFailure(const std::string& devname) override;


   private:
      int appResult;
      int argc;
      char** argv;

      Config* cfg;
      LogContext* log;

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      StringList* allowedInterfaces; // empty list means "all interfaces accepted"
      std::shared_ptr<Node> localNode;

      NodeStoreServers* mgmtNodes;
      NodeStoreServers* metaNodes;
      NodeStoreServers* storageNodes;
      NodeStoreClients* clientNodes;

      RootInfo metaRoot;

      TargetMapper* targetMapper;
      MirrorBuddyGroupMapper* mirrorBuddyGroupMapper;
      MirrorBuddyGroupMapper* metaMirrorBuddyGroupMapper;
      TargetStateStore* targetStateStore;
      TargetStateStore* metaTargetStateStore;
      std::unique_ptr<StoragePoolStore> storagePoolStore;

      MultiWorkQueue* workQueue;
      AcknowledgmentStore* ackStore;
      NetMessageFactory* netMessageFactory;

      DatagramListener* dgramListener;
      WorkerList workerList;

      void runNormal();

      int executeMode(const RunModesElem* runMode);

      void workersInit();
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initDataObjects();
      void initLocalNodeInfo();
      void initComponents();
      void startComponents();
      void joinComponents();

      void logInfos();

      void daemonize();

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

      StoragePoolStore* getStoragePoolStore() const
      {
         return storagePoolStore.get();
      }

      MultiWorkQueue* getWorkQueue() const
      {
         return workQueue;
      }

      AcknowledgmentStore* getAckStore() const
      {
         return ackStore;
      }

      DatagramListener* getDatagramListener() const
      {
         return dgramListener;
      }

      int getAppResult() const
      {
         return appResult;
      }

      const RootInfo& getMetaRoot() const { return metaRoot; }
      RootInfo& getMetaRoot() { return metaRoot; }
};


#endif /*APP_H_*/
