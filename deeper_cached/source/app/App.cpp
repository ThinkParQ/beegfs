#include <common/app/log/LogContext.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/ComponentInitException.h>
#include <common/components/RegistrationDatagramListener.h>
#include <common/cache/toolkit/threadvartk.h>
#include <common/nodes/NodeType.h>
#include <opentk/logging/SyslogLogger.h>
#include <program/Program.h>
#include "App.h"

#include <signal.h>
#include <sys/resource.h>


#define APP_WORKERS_DIRECT_NUM      3
#define APP_SYSLOG_IDENTIFIER       "beegfs-deeper-cached"


App::App(int argc, char** argv)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;
   this->pidFileLockFD = -1;

   this->cfg = NULL;
   this->log = NULL;

   this->workQueue = NULL;

   this->connAcceptor = NULL;
   this->netMessageFactory = NULL;

   this->nextNumaBindTarget = 0;

   this->healthChecker = NULL;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   workersDelete();

   // delete the named socket file after all workers are finished
   unlink(cfg->getConnNamedSocket().c_str() );

   if(cfg->getLogHealthCheckIntervalSec() )
      SAFE_DELETE(this->healthChecker);

   SAFE_DELETE(this->connAcceptor);
   SAFE_DELETE(this->netMessageFactory);

   streamListenersDelete();

   SAFE_DELETE(this->workQueue);

   SAFE_DELETE(this->log);

   unlockAndDeletePIDFile(pidFileLockFD, cfg->getPIDFile()); // ignored if fd is -1

   SAFE_DELETE(this->cfg);

   Logger::destroyLogger();
   SyslogLogger::destroyOnce();
}

/**
 * Initialize config and run app either in normal mode or in special unit tests mode.
 */
void App::run()
{
   try
   {
      SyslogLogger::initOnce(APP_SYSLOG_IDENTIFIER);

      this->cfg = new Config(argc, argv);

      verifyConfig();

      std::string nodeID = System::getHostname();
      this->localNode = std::make_shared<LocalNode>(nodeID, NumNodeID(0), 0, 0,
         this->cfg->getConnNamedSocket(), NODETYPE_CacheDaemon); // use NodeType of destination node

      runNormal();
   }
   catch (InvalidConfigException& e)
   {
      std::cerr << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
      std::cerr << std::endl;
      std::cerr << "[BeeGFS DEEP-ER cache daemon Version: " << BEEGFS_VERSION << std::endl;
      std::cerr << "Refer to the default config file (/etc/beegfs/beegfs-deeper-cached.conf)"
         << std::endl;
      std::cerr << "or visit http://www.beegfs.com to find out about configuration options.]"
         << std::endl;
      std::cerr << std::endl;

      if(this->log)
         log->logErr(e.what() );

      appResult = APPCODE_INVALID_CONFIG;
      return;
   }
   catch (std::exception& e)
   {
      std::cerr << std::endl;
      std::cerr << "Unrecoverable error: " << e.what() << std::endl;
      std::cerr << std::endl;

      if(this->log)
         log->logErr(e.what() );

      appResult = APPCODE_RUNTIME_ERROR;
      return;
   }
}

/**
 * @throw InvalidConfigException on error
 */
void App::runNormal()
{
   // numa binding (as early as possible)

   if(cfg->getTuneBindToNumaZone() != -1) // -1 means disable binding
   {
      bool bindRes = System::bindToNumaNode(cfg->getTuneBindToNumaZone() );
      if(!bindRes)
         throw InvalidConfigException("Unable to bind to this NUMA zone: " +
            StringTk::intToStr(cfg->getTuneBindToNumaZone() ) );
   }


   // init basic data objects

   initLogging();
   initDataObjects();

   // prepare factory for incoming messages
   this->netMessageFactory = new NetMessageFactory();

   registerSignalHandler();


   // detach process

   if(cfg->getRunDaemonized() )
      daemonize();


   // init components

   try
   {
      initComponents();
   }
   catch(ComponentInitException& e)
   {
      log->logErr(e.what() );
      log->log(Log_CRITICAL, "A hard error occurred. Shutting down...");
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }


   // log system and configuration info

   logInfos();


   // start component threads and join them

   startComponents();


   // set permissions on named socket, permissions 777

   if(chmod(cfg->getConnNamedSocket().c_str(), S_IRWXU | S_IRWXG | S_IRWXO) )
   {
      log->log(Log_CRITICAL, "Can not change permission of named socket! Path: " +
         cfg->getConnNamedSocket() );
   }


   // wait for termination

   joinComponents();

   log->log(Log_CRITICAL, "All components stopped. Exiting now!");
}

void App::initLogging()
{
   // check absolute log path to avoid chdir() problems
   Path logStdPath(cfg->getLogStdFile() );
   Path logErrPath(cfg->getLogErrFile() );

   if( (!logStdPath.empty() && !logStdPath.absolute() ) ||
       (!logErrPath.empty() && !logErrPath.absolute() ) )
   {
      throw InvalidConfigException("Path to log file must be absolute");
   }

   Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogErrsToStdlog(), 
      cfg->getLogNoDate(), cfg->getLogStdFile(), cfg->getLogErrFile(), cfg->getLogNumLines(),
      cfg->getLogNumRotatedFiles());
   this->log = new LogContext("App");
}

/**
 * Init basic shared objects like work queues, node stores etc.
 */
void App::initDataObjects()
{
   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   this->workQueue = new CacheWorkManager();
}

void App::initComponents()
{
   this->log->log(Log_DEBUG, "Initializing components...");

   streamListenersInit();

   this->connAcceptor = new ConnAcceptor(this, this->cfg->getConnNamedSocket() );

   workersInit();

   if(cfg->getLogHealthCheckIntervalSec() )
      this->healthChecker = new HealthChecker();

   this->log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   PThread::blockInterruptSignals(); // reblock signals for next child threads

   streamListenersStart();

   this->connAcceptor->start();

   workersStart();

   if(cfg->getLogHealthCheckIntervalSec() )
      this->healthChecker->start();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   // note: no commslave stop here, because that would keep workers from terminating
   if(cfg->getLogHealthCheckIntervalSec() )
      this->healthChecker->selfTerminate();

   workersStop();

   if(connAcceptor)
      connAcceptor->selfTerminate();

   streamListenersStop();

   this->selfTerminate(); /* this flag can be noticed by thread-independent methods and is also
      required e.g. to let App::waitForMgmtNode() know that it should cancel */
}

/**
 * Handles expections that lead to the termination of a component.
 * Initiates an application shutdown.
 */
void App::handleComponentException(std::exception& e)
{
   const char* logContext = "App (component exception handler)";
   LogContext log(logContext);

   log.logErr(std::string("This component encountered an unrecoverable error. ") +
      std::string("; [SysErr: ") + System::getErrString() + "] " +
      std::string("; Exception message: ") + e.what() );

   log.log(Log_WARNING, "Shutting down...");

   stopComponents();
}


void App::joinComponents()
{
   log->log(Log_DEBUG, "Joining component threads...");

   workersJoin();

   if(cfg->getLogHealthCheckIntervalSec() )
      waitForComponentTermination(healthChecker);

   waitForComponentTermination(connAcceptor);

   streamListenersJoin();
}

void App::streamListenersInit()
{
   this->numStreamListeners = cfg->getTuneNumStreamListeners();

   for(unsigned i=0; i < numStreamListeners; i++)
   {
      StreamListenerV2* listener = new StreamListenerV2(
         std::string("StreamLis") + StringTk::uintToStr(i+1), this, workQueue);

      if(cfg->getTuneListenerPrioShift() )
         listener->setPriorityShift(cfg->getTuneListenerPrioShift() );

      if(cfg->getTuneUseAggressiveStreamPoll() )
         listener->setUseAggressivePoll();

      streamLisVec.push_back(listener);
   }
}

void App::workersInit()
{
   unsigned numWorkers = cfg->getTuneNumWorkers();

   for(unsigned i=0; i < numWorkers; i++)
   {
      CacheWorker* worker = new CacheWorker(std::string("CacheWorker") + StringTk::uintToStr(i+1),
         workQueue, QueueWorkType_INDIRECT);

      worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

      workerList.push_back(worker);
   }

   for(unsigned i=0; i < APP_WORKERS_DIRECT_NUM; i++)
   {
      CacheWorker* worker = new CacheWorker(std::string("DirectWorker") + StringTk::uintToStr(i+1),
         workQueue, QueueWorkType_DIRECT);

      worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

      workerList.push_back(worker);
   }
}

void App::streamListenersStart()
{
   unsigned numNumaNodes = System::getNumNumaNodes();

   for(StreamLisVecIter iter = streamLisVec.begin(); iter != streamLisVec.end(); iter++)
   {
      if(cfg->getTuneListenerNumaAffinity() )
         (*iter)->startOnNumaNode( (++nextNumaBindTarget) % numNumaNodes);
      else
         (*iter)->start();
   }
}

void App::workersStart()
{
   unsigned numNumaNodes = System::getNumNumaNodes();

   for(CacheWorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      if(cfg->getTuneWorkerNumaAffinity() )
         (*iter)->startOnNumaNode( (++nextNumaBindTarget) % numNumaNodes);
      else
         (*iter)->start();
   }
}

void App::streamListenersStop()
{
   for(StreamLisVecIter iter = streamLisVec.begin(); iter != streamLisVec.end(); iter++)
   {
      (*iter)->selfTerminate();
   }
}

void App::workersStop()
{
   for(CacheWorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      (*iter)->selfTerminate();
   }

   for(CacheWorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      workQueue->addDirectWork(new DummyWork() );
   }
}

void App::streamListenersDelete()
{
   for(StreamLisVecIter iter = streamLisVec.begin(); iter != streamLisVec.end(); iter++)
   {
      delete(*iter);
   }

   streamLisVec.clear();
}

void App::workersDelete()
{
   for(CacheWorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      delete(*iter);
   }

   workerList.clear();
}

void App::streamListenersJoin()
{
   for(StreamLisVecIter iter = streamLisVec.begin(); iter != streamLisVec.end(); iter++)
   {
      waitForComponentTermination(*iter);
   }
}

void App::workersJoin()
{
   /* (note: we need one thread for which we do an untimed join, so this should be a quite reliably
      terminating thread) */
   CacheWorkerListIter iter = workerList.begin();
   (*iter)->join();
   iter++;

   for(; iter != workerList.end(); iter++)
   {
      waitForComponentTermination(*iter);
   }
}

void App::logInfos()
{
   log->log(Log_CRITICAL, "BeeGFS DEEP-ER cache daemon");
   log->log(Log_CRITICAL, std::string("Version: ") + BEEGFS_VERSION);
   log->log(Log_CRITICAL, "Cache FS path: " + cfg->getSysMountPointCache() );
   log->log(Log_CRITICAL, "Global FS Path: " + cfg->getSysMountPointGlobal() );

   // print debug version info
   LOG_DEBUG_CONTEXT(*log, Log_CRITICAL, "--DEBUG VERSION--");

   // print numa info
   // (getTuneBindToNumaZone==-1 means disable binding)
   if(cfg->getTuneListenerNumaAffinity() || cfg->getTuneWorkerNumaAffinity() ||
      (cfg->getTuneBindToNumaZone() != -1) )
   {
      unsigned numNumaNodes = System::getNumNumaNodes();

      /* note: we use the term "numa areas" instead of "numa nodes" in log messages to avoid
         confusion with cluster "nodes" */

      log->log(Log_NOTICE, std::string("NUMA areas: ") + StringTk::uintToStr(numNumaNodes) );

      for(unsigned nodeNum=0; nodeNum < numNumaNodes; nodeNum++)
      { // print core list for each numa node
         cpu_set_t cpuSet;

         System::getNumaCoresByNode(nodeNum, &cpuSet);

         // create core list for current numa node

         std::string coreListStr;
         for(unsigned coreNum = 0; coreNum < CPU_SETSIZE; coreNum++)
         {
            if(CPU_ISSET(coreNum, &cpuSet) )
               coreListStr += StringTk::uintToStr(coreNum) + " ";
         }

         log->log(Log_SPAM, "NUMA area " + StringTk::uintToStr(nodeNum) + " cores: " + coreListStr);
      }
   }
}

void App::daemonize()
{
   int nochdir = 1; // 1 to keep working directory
   int noclose = 0; // 1 to keep stdin/-out/-err open

   log->log(Log_DEBUG, std::string("Detaching process...") );

   int detachRes = daemon(nochdir, noclose);
   if(detachRes == -1)
      throw InvalidConfigException(std::string("Unable to detach process. SysErr: ") +
         System::getErrString() );

   updateLockedPIDFile(pidFileLockFD); // ignored if pidFileFD is -1
}

void App::registerSignalHandler()
{
   signal(SIGINT, App::signalHandler);
   signal(SIGTERM, App::signalHandler);
}

void App::signalHandler(int sig)
{
   App* app = Program::getApp();

   Logger* log = Logger::getLogger();
   const char* logContext = "App::signalHandler";

   // note: this might deadlock if the signal was thrown while the logger mutex is locked by the
   //    application thread (depending on whether the default mutex style is recursive). but
   //    even recursive mutexes are not acceptable in this case.
   //    we need something like a timed lock for the log mutex. if it succeeds within a
   //    few seconds, we know that we didn't hold the mutex lock. otherwise we simply skip the
   //    log message. this will only work if the mutex is non-recusive (which is unknown for
   //    the default mutex style).
   //    but it is very unlikely that the application thread holds the log mutex, because it
   //    joins the component threads and so it doesn't do anything else but sleeping!

   switch(sig)
   {
      case SIGINT:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         log->log(Log_CRITICAL, logContext, "Received a SIGINT. Shutting down...");
      } break;

      case SIGTERM:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         log->log(Log_CRITICAL, logContext, "Received a SIGTERM. Shutting down...");
      } break;

      default:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         log->log(Log_CRITICAL, logContext, "Received an unknown signal. Shutting down...");
      } break;
   }

   app->stopComponents();
}
