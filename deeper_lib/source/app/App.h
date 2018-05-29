#ifndef APP_H_
#define APP_H_


#include <app/config/Config.h>
#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
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


// forward declarations
class LogContext;


/**
 * This is not a real BeeGFS App, it is only a container for the configuration and the logger, so we
 * can use the default mechanism to get the logger or the configuration everywhere in the program.
 */
class App : public AbstractApp
{
   public:
      App(int argc, char** argv)
      {
         this->argc = argc;
         this->argv = argv;

         appResult = APPCODE_NO_ERROR;
         pidFileLockFD = -1;

         cfg = new Config(argc, argv);
         Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogErrsToStdlog(), 
            cfg->getLogNoDate(), cfg->getLogStdFile(), cfg->getLogErrFile(), cfg->getLogNumLines(),
            cfg->getLogNumRotatedFiles());

         verifyConfig();

         Logger::getLogger()->log(Log_DEBUG, "beegfs-deeper-lib", "Version: " BEEGFS_VERSION);
         Logger::getLogger()->log(Log_DEBUG, "beegfs-deeper-lib", "Cache ID: " +
            StringTk::uint64ToStr(cfg->getSysCacheID() ) );
         Logger::getLogger()->log(Log_DEBUG, "beegfs-deeper-lib", "Cache FS path: " +
            cfg->getSysMountPointCache() );
         Logger::getLogger()->log(Log_DEBUG, "beegfs-deeper-lib", "Global FS Path: " +
            cfg->getSysMountPointGlobal() );

         netMessageFactory = new NetMessageFactory();
      };

      virtual ~App()
      {
         SAFE_DELETE(netMessageFactory);

         SAFE_DELETE(log);

         SAFE_DELETE(cfg);

         Logger::destroyLogger();
      };

      virtual void run() {};

      void stopComponents() {};
      void handleComponentException(std::exception& e) {};


   private:
      int appResult;
      int argc;
      char** argv;

      Config*  cfg;
      LogContext* log;

      int pidFileLockFD; // -1 if unlocked, >=0 otherwise

      NetMessageFactory* netMessageFactory;

      void verifyConfig()
      {
         if(cfg->getSysMountPointCache().empty() )
            Logger::getLogger()->logErr("beegfs-deeper-lib", "No path to global FS specified.");

         if(cfg->getSysMountPointCache().empty() )
            Logger::getLogger()->logErr("beegfs-deeper-lib", "No path to cache FS specified.");

         if(cfg->getConnNamedSocket().empty() )
            Logger::getLogger()->logErr("beegfs-deeper-lib",
               "No path to named socket for the communication specified.");
      }


   public:
      // inliners

      // getters & setters

      virtual ICommonConfig* getCommonConfig()
      {
         return cfg;
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
         return netMessageFactory;
      }

      Config* getConfig() const
      {
         return cfg;
      }

      int getAppResult() const
      {
         return appResult;
      }
};


#endif /*APP_H_*/
