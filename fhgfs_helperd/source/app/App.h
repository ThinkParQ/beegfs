#ifndef APP_H_
#define APP_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/Worker.h>
#include <common/components/StreamListener.h>
#include <common/nodes/Node.h>
#include <common/toolkit/NetFilter.h>
#include <common/Common.h>
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

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      std::shared_ptr<Node> localNode;

      MultiWorkQueue* workQueue;
      NetMessageFactory* netMessageFactory;

      StreamListener* streamListener;
      WorkerList workerList;

      TestRunner *testRunner;

      void runNormal();
      void runUnitTests();

      void workersInit() throw(ComponentInitException);
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initDataObjects(int argc, char** argv) throw(InvalidConfigException);
      void initLocalNodeInfo() throw(InvalidConfigException);
      void initStorage() throw(InvalidConfigException);
      void initComponents() throw(ComponentInitException);
      void startComponents();
      void joinComponents();

      void logInfos();

      void daemonize() throw(InvalidConfigException);

      void registerSignalHandler();
      static void signalHandler(int sig);

   public:
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

      StreamListener* getStreamListener() const
      {
         return streamListener;
      }

      int getAppResult() const
      {
         return appResult;
      }

};


#endif /*APP_H_*/
