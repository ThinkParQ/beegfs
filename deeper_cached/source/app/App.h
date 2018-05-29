#ifndef APP_H_
#define APP_H_


#include <app/config/Config.h>
#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/components/streamlistenerv2/ConnAcceptor.h>
#include <common/components/streamlistenerv2/StreamListenerV2.h>
#include <common/nodes/LocalNode.h>
#include <components/HealthChecker.h>
#include <components/worker/CacheWorker.h>
#include <components/worker/queue/CacheWorkManager.h>
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
#define APPCODE_RUNTIME_ERROR          3


typedef std::list<CacheWorker*> CacheWorkerList;
typedef CacheWorkerList::iterator CacheWorkerListIter;

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

      Config*  cfg;
      LogContext* log;

      int pidFileLockFD; // -1 if unlocked, >=0 otherwise

      CacheWorkManager* workQueue;

      std::shared_ptr<Node> localNode;
      ConnAcceptor* connAcceptor;
      NetMessageFactory* netMessageFactory;

      unsigned numStreamListeners; // value copied from cfg (for performance)
      StreamLisVec streamLisVec;

      CacheWorkerList workerList;

      unsigned nextNumaBindTarget; // the numa node to which we will bind the next component thread

      HealthChecker* healthChecker;


      void runNormal();

      void streamListenersInit();
      void streamListenersStart();
      void streamListenersStop();
      void streamListenersDelete();
      void streamListenersJoin();

      void workersInit();
      void workersStart();
      void workersStop();
      void workersDelete();
      void workersJoin();

      void initLogging();
      void initDataObjects();
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
       * Get one of the available stream listeners based on the socket file descriptor number.
       * This is to load-balance the sockets over all available stream listeners and ensure that
       * sockets are not bouncing between different stream listeners.
       *
       * Note that IB connections eat two fd numbers, so 2 and multiples of 2 might not be a good
       * value for number of stream listeners.
       */
      virtual StreamListenerV2* getStreamListenerByFD(int fd)
      {
         return this->streamLisVec[fd % numStreamListeners];
      }

      // getters & setters

      virtual ICommonConfig* getCommonConfig()
      {
         return this->cfg;
      }

      virtual NetFilter* getNetFilter()
      {
         return NULL;
      }

      virtual NetFilter* getTcpOnlyFilter()
      {
         return NULL;
      }

      virtual AbstractNetMessageFactory* getNetMessageFactory()
      {
         return this->netMessageFactory;
      }

      Config* getConfig() const
      {
         return this->cfg;
      }

      CacheWorkerList* getWorkers()
      {
         return &this->workerList;
      }

      int getAppResult() const
      {
         return this->appResult;
      }

      CacheWorkManager* getWorkQueue() const
      {
         return this->workQueue;
      }

      Node& getLocalNode()
      {
         return *this->localNode;
      }

      void verifyConfig()
      {
         if(cfg->getSysMountPointCache().empty() )
            throw InvalidConfigException("No path to global FS specified.");

         if(cfg->getSysMountPointCache().empty() )
            throw InvalidConfigException("No path to cache FS specified.");

         if(cfg->getConnNamedSocket().empty() )
            throw InvalidConfigException(
               "No path to named socket for the communication specified.");
      }
};


#endif /*APP_H_*/
