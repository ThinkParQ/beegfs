#ifndef APP_H_
#define APP_H_

#include <app/Config.h>
#include <common/app/AbstractApp.h>
#include <common/app/log/Logger.h>
#include <common/Common.h>
#include <common/components/worker/Worker.h>
#include <common/nodes/LocalNode.h>
#include <common/nodes/NodeStoreClients.h>
#include <common/nodes/Node.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NetFilter.h>
#include <common/toolkit/NodesTk.h>
#include <misc/TSDatabase.h>
#include <components/CleanUp.h>
#include <components/StatsCollector.h>
#include <components/NodeListRequestor.h>
#include <net/message/NetMessageFactory.h>
#include <nodes/NodeStoreMetaEx.h>
#include <nodes/NodeStoreStorageEx.h>
#include <nodes/NodeStoreMgmtEx.h>

class App : public AbstractApp
{
   public:
      enum AppCode
      {
         NO_ERROR = 0,
         INVALID_CONFIG = 1,
         INITIALIZATION_ERROR = 2,
         RUNTIME_ERROR = 3
      };


      App(int argc, char** argv);

      virtual void run() override;
      virtual void stopComponents() override;
      virtual void handleComponentException(std::exception& e) override;
      virtual void handleNetworkInterfaceFailure(const std::string& devname) override;


   private:
      int appResult;
      int argc;
      char** argv;
      LockFD pidFileLockFD;

      std::unique_ptr<TargetMapper> targetMapper;

      std::unique_ptr<Config> cfg;
      std::unique_ptr<NetFilter> netFilter;
      std::unique_ptr<NetFilter> tcpOnlyFilter;
      std::unique_ptr<NetMessageFactory> netMessageFactory;
      NicListCapabilities localNicCaps;
      std::shared_ptr<Node> localNode;
      std::unique_ptr<TSDatabase> tsdb;
      std::unique_ptr<MultiWorkQueue> workQueue;

      std::unique_ptr<NodeStoreMgmtEx> mgmtNodes;
      std::unique_ptr<NodeStoreMetaEx> metaNodes;
      std::unique_ptr<NodeStoreStorageEx> storageNodes;
      std::unique_ptr<MirrorBuddyGroupMapper> metaBuddyGroupMapper;
      std::unique_ptr<MirrorBuddyGroupMapper> storageBuddyGroupMapper;

      std::unique_ptr<NodeListRequestor> nodeListRequestor;
      std::unique_ptr<StatsCollector> statsCollector;
      std::unique_ptr<CleanUp> cleanUp;

      std::list<std::unique_ptr<Worker>> workerList;

      void printOrLogError(const std::string& text) const;

      void runNormal();
      void initDataObjects();
      void initComponents();
      void startComponents();
      void joinComponents();
      void initWorkers();
      void startWorkers();
      void stopWorkers();
      void joinWorkers();
      void initLocalNodeInfo();
      void logInfos();
      void daemonize();

   public:
      NodeStoreServers* getServerStoreFromType(NodeType nodeType)
      {
         switch (nodeType)
         {
            case NODETYPE_Meta:
               return metaNodes.get();

            case NODETYPE_Storage:
               return storageNodes.get();

            case NODETYPE_Mgmt:
               return mgmtNodes.get();

            default:
               return nullptr;
         }
      }

      virtual ICommonConfig* getCommonConfig() const override
      {
         return cfg.get();
      }

      virtual NetFilter* getNetFilter() const override
      {
         return netFilter.get();
      }

      virtual NetFilter* getTcpOnlyFilter() const override
      {
         return tcpOnlyFilter.get();
      }

      virtual AbstractNetMessageFactory* getNetMessageFactory() const override
      {
         return netMessageFactory.get();
      }

      std::shared_ptr<Node> getLocalNode()
      {
         return localNode;
      }

      Config* getConfig()
      {
         return cfg.get();
      }

      MultiWorkQueue *getWorkQueue()
      {
         return workQueue.get();
      }

      NodeStoreMetaEx *getMetaNodes()
      {
         return metaNodes.get();
      }

      NodeStoreStorageEx *getStorageNodes()
      {
         return storageNodes.get();
      }

      NodeStoreMgmtEx *getMgmtNodes()
      {
         return mgmtNodes.get();
      }

      TSDatabase *getTSDB()
      {
         return tsdb.get();
      }

      TargetMapper* getTargetMapper()
      {
         return targetMapper.get();
      }

      MirrorBuddyGroupMapper* getMetaBuddyGroupMapper()
      {
         return metaBuddyGroupMapper.get();
      }

      MirrorBuddyGroupMapper* getStorageBuddyGroupMapper()
      {
         return storageBuddyGroupMapper.get();
      }

      int getAppResult()
      {
         return appResult;
      }
};


#endif /*APP_H_*/
