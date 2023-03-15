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
      virtual void handleNetworkInterfaceFailure(const std::string& devname) override;


   private:
      int appResult;
      int argc;
      char** argv;

      Config*  cfg;
      LogContext* log;
      std::list<std::string> allowedInterfaces;

      LockFD pidFileLockFD;

      NetFilter* netFilter; // empty filter means "all nets allowed"
      NetFilter* tcpOnlyFilter; // for IPs that allow only plain TCP (no RDMA etc)
      std::shared_ptr<Node> localNode;

      MultiWorkQueue* workQueue;
      NetMessageFactory* netMessageFactory;

      StreamListener* streamListener;
      WorkerList workerList;

      void runNormal();

      void workersInit();
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initDataObjects(int argc, char** argv);
      void initLocalNodeInfo();
      void initStorage();
      void initComponents();
      void startComponents();
      void joinComponents();

      void logInfos();

      void daemonize();

      void registerSignalHandler();
      static void signalHandler(int sig);

   public:
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
