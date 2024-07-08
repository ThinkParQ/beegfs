#ifndef ABSTRACTAPP_H_
#define ABSTRACTAPP_H_

#include <common/app/log/Logger.h>
#include <common/app/config/ICommonConfig.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/threading/PThread.h>
#include <common/toolkit/BitStore.h>
#include <common/toolkit/NetFilter.h>
#include <common/toolkit/LockFD.h>
#include <common/Common.h>
#include <common/net/sock/RoutingTable.h>

#include <mutex>

#include <mutex>

class StreamListenerV2; // forward declaration
class LogContext;
class AbstractNodeStore;

class AbstractApp : public PThread
{
   public:
      virtual void stopComponents() = 0;
      virtual void handleComponentException(std::exception& e) = 0;
      virtual void handleNetworkInterfaceFailure(const std::string& devname) = 0;

      LockFD createAndLockPIDFile(const std::string& pidFile);
      void updateLockedPIDFile(LockFD& pidFileFD);

      void waitForComponentTermination(PThread* component);

      void logUsableNICs(LogContext* log, NicAddressList& nicList);

      static void handleOutOfMemFromNew();

      virtual const ICommonConfig* getCommonConfig() const = 0;
      virtual const NetFilter* getNetFilter() const = 0;
      virtual const NetFilter* getTcpOnlyFilter() const = 0;
      virtual const AbstractNetMessageFactory* getNetMessageFactory() const = 0;


      static bool didRunTimeInit;


      /**
       * To be overridden by Apps that support multiple stream listeners.
       */
      virtual StreamListenerV2* getStreamListenerByFD(int fd)
      {
         return NULL;
      }

      NicAddressList getLocalNicList()
      {
         const std::lock_guard<Mutex> lock(localNicListMutex);
         return localNicList;
      }

   protected:
      NicAddressList localNicList; // intersection set of dicsovered NICs and allowedInterfaces
      Mutex localNicListMutex;
      std::shared_ptr<NetVector> noDefaultRouteNets;

      AbstractApp() : PThread("Main", this),
                      noDefaultRouteNets(nullptr)
      {
         if (!didRunTimeInit)
         {
            std::cerr << "Bug: Runtime variables have not been initialized" << std::endl;
            throw InvalidConfigException("Bug: Runtime variables have not been initialized");
         }

         std::set_new_handler(handleOutOfMemFromNew);
      }

      virtual ~AbstractApp()
      {
         basicDestructions();
      }

      void updateLocalNicListAndRoutes(LogContext* log, NicAddressList& nicList,
         std::vector<AbstractNodeStore*>& nodeStores);

      bool initNoDefaultRouteList(NetVector* outNets);

      void logEULAMsg(std::string);

   private:
      static bool basicInitializations();
      bool basicDestructions();
      static bool performBasicInitialRunTimeChecks();
      RoutingTableFactory routingTableFactory;

      // inliners

   public:

      /**
       * Note: This MUST be called before creating a new App
       */
      static bool runTimeInitsAndChecks()
      {
         bool runTimeChecks = performBasicInitialRunTimeChecks();
         if (!runTimeChecks)
            return false;

         bool initRes = basicInitializations();
         if (!initRes)
            return false;

         didRunTimeInit = true;

         return true;
      }

      void initRoutingTable()
      {
         routingTableFactory.init();
      }

      bool updateRoutingTable()
      {
         return routingTableFactory.load();
      }

      RoutingTable getRoutingTable()
      {
         return routingTableFactory.create(noDefaultRouteNets);
      }

};

#endif /*ABSTRACTAPP_H_*/
