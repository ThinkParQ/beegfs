#ifndef APP_H_
#define APP_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/nodes/NodeStore.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <common/toolkit/NodesTk.h>
#include <common/Common.h>
#include <common/components/worker/Worker.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/ComponentInitException.h>
#include <common/net/sock/RDMASocket.h>
#include <common/nodes/TargetMapper.h>
#include <common/nodes/LocalNode.h>
#include <components/DatagramListener.h>
#include <components/InternodeSyncer.h>
#include <components/ModificationEventHandler.h>
#include <modes/ModeHelp.h>
#include <modes/ModeCheckFS.h>
#include <modes/ModeEnableQuota.h>
#include <net/message/NetMessageFactory.h>


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
#define APPCODE_COMMUNICATION_ERROR    3
#define APPCODE_RUNTIME_ERROR          4
#define APPCODE_USER_ABORTED           5

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

      void abort();
      void handleComponentException(std::exception& e);

      bool getShallAbort()
      {
         return (shallAbort.read() != 0);
      }

   private:
      int appResult;
      int argc;
      char** argv;

      AtomicSizeT shallAbort;

      Config* cfg;
      LogContext* log;

      NetFilter* netFilter;
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      StringList* allowedInterfaces;
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces

      std::shared_ptr<Node> localNode;
      NodeStore* mgmtNodes;
      NodeStore* metaNodes;
      NodeStore* storageNodes;
      InternodeSyncer* internodeSyncer;
      TargetMapper* targetMapper;
      TargetStateStore* targetStateStore;
      MirrorBuddyGroupMapper* buddyGroupMapper;
      MirrorBuddyGroupMapper* metaBuddyGroupMapper;
      MultiWorkQueue* workQueue;
      AcknowledgmentStore* ackStore;
      NetMessageFactory* netMessageFactory;

      DatagramListener* dgramListener;
      WorkerList workerList;

      ModificationEventHandler* modificationEventHandler;

      Mode* runMode;

      void runNormal();

      void workersInit() throw (ComponentInitException);
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();
      void initDataObjects(int argc, char** argv);
      void initLocalNodeInfo();
      void initComponents();
      bool initRunMode();
      void startComponents();
      void joinComponents();
      void stopComponents();
      void logInfos();
      bool waitForMgmtNode();
      void registerSignalHandler();
      static void signalHandler(int sig);

   public:
      virtual ICommonConfig* getCommonConfig()
      {
         return cfg;
      }

      virtual AbstractNetMessageFactory* getNetMessageFactory()
      {
         return netMessageFactory;
      }

      virtual NetFilter* getNetFilter()
      {
         return netFilter;
      }

      virtual NetFilter* getTcpOnlyFilter()
      {
         return tcpOnlyFilter;
      }

      Config* getConfig() const
      {
         return cfg;
      }

      DatagramListener* getDatagramListener() const
      {
         return dgramListener;
      }

      int getAppResult() const
      {
         return appResult;
      }

      NodeStore* getMgmtNodes() const
      {
         return mgmtNodes;
      }

      NodeStore* getMetaNodes()
      {
         return metaNodes;
      }

      NodeStore* getStorageNodes() const
      {
         return storageNodes;
      }

      Node& getLocalNode() const
      {
         return *localNode;
      }

      MultiWorkQueue* getWorkQueue()
      {
         return workQueue;
      }

      InternodeSyncer* getInternodeSyncer()
      {
         return internodeSyncer;
      }

      TargetMapper* getTargetMapper()
      {
         return targetMapper;
      }

      TargetStateStore* getTargetStateStore()
      {
         return targetStateStore;
      }

      MirrorBuddyGroupMapper* getMirrorBuddyGroupMapper()
      {
         return buddyGroupMapper;
      }

      MirrorBuddyGroupMapper* getMetaMirrorBuddyGroupMapper()
      {
         return metaBuddyGroupMapper;
      }

      ModificationEventHandler* getModificationEventHandler()
      {
         return modificationEventHandler;
      }

      void setModificationEventHandler(ModificationEventHandler* handler)
      {
         modificationEventHandler = handler;
      }
};

#endif // APP_H_
