#include <common/app/log/LogContext.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/ComponentInitException.h>
#include <common/net/sock/RDMASocket.h>
#include <common/nodes/LocalNode.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <opentk/logging/SyslogLogger.h>
#include <program/Program.h>
#include "App.h"

#include <signal.h>


#define APP_WORKERS_DIRECT_NUM   1
#define APP_WORKERS_BUF_SIZE     (1024*1024)
#define APP_SYSLOG_IDENTIFIER    "beegfs-helperd"


App::App(int argc, char** argv)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;
   this->pidFileLockFD = -1;

   this->cfg = NULL;
   this->netFilter = NULL;
   this->tcpOnlyFilter = NULL;
   this->log = NULL;
   this->workQueue = NULL;
   this->netMessageFactory = NULL;

   this->streamListener = NULL;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   workersDelete();

   SAFE_DELETE(this->streamListener);
   SAFE_DELETE(this->netMessageFactory);
   SAFE_DELETE(this->workQueue);
   this->localNode.reset();
   SAFE_DELETE(this->log);
   SAFE_DELETE(this->tcpOnlyFilter);
   SAFE_DELETE(this->netFilter);

   unlockAndDeletePIDFile(pidFileLockFD, cfg->getPIDFile()); // ignored if fd is -1

   SAFE_DELETE(this->cfg);

   Logger::destroyLogger();
   SyslogLogger::destroyOnce();
}

void App::run()
{
   try
   {
      SyslogLogger::initOnce(APP_SYSLOG_IDENTIFIER);

      this->cfg = new Config(argc, argv);

      runNormal();
   }
   catch (InvalidConfigException& e)
   {
      std::cerr << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
      std::cerr << std::endl;
      std::cerr << "[BeeGFS Helper Daemon Version: " << BEEGFS_VERSION << std::endl;
      std::cerr << "Refer to the default config file (/etc/beegfs/beegfs-helperd.conf)" << std::endl;
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

void App::runNormal()
{
   // init data objects & storage
   initDataObjects(argc, argv);
   //initStorage(); // included in initDataObjects now

   // init components

   try
   {
      initComponents();
   }
   catch (ComponentInitException& e)
   {
      log->logErr(e.what());
      log->log(1, "A hard error occurred. Shutting down...");
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }

   // log system and configuration info

   logInfos();

   // detach process
   // prepare ibverbs for forking
   RDMASocket::rdmaForkInitOnce();

   // detach process
   try
   {
      if ( this->cfg->getRunDaemonized() )
         daemonize();
   }
   catch (InvalidConfigException& e)
   {
      log->logErr(e.what());
      log->log(1, "A hard error occurred. Shutting down...");
      appResult = APPCODE_INVALID_CONFIG;
      return;
   }

   // find RDMA interfaces (based on TCP/IP interfaces)

   // note: we do this here, because when we first create an RDMASocket (and this is done in this
   // check), the process opens the verbs device. Recent OFED versions have a check if the
   // credentials of the opening process match those of the calling process (not only the values
   // are compared, but the pointer is checked for equality). Thus, the first open needs to happen
   // after the fork, because we need to access the device in the child process.
   if(cfg->getConnUseRDMA() && RDMASocket::rdmaDevicesExist() )
   {
      bool foundRdmaInterfaces = NetworkInterfaceCard::checkAndAddRdmaCapability(localNicList);

      if (foundRdmaInterfaces)
         localNicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp); // re-sort the niclist
   }

   // start component threads

   if ( this->cfg->getDebugRunComponentThreads() )
   {
      startComponents();

      // wait for termination

      joinComponents();
   }

   log->log(1, "All components stopped. Exiting now!");
}

void App::initDataObjects(int argc, char** argv)
{
  // this->cfg = new Config(argc, argv);

   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   this->netFilter = new NetFilter(cfg->getConnNetFilterFile() );
   this->tcpOnlyFilter = new NetFilter(cfg->getConnTcpOnlyFilterFile() );

   // (check absolute log path to avoid chdir() problems)
   Path logStdPath(cfg->getLogStdFile() );
   Path logErrPath(cfg->getLogErrFile() );
   if( (!logStdPath.empty() && !logStdPath.absolute() ) ||
       (!logErrPath.empty() && !logErrPath.absolute() ) )
      throw InvalidConfigException("Path to log file must be absolute");

   Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogErrsToStdlog(), 
      cfg->getLogNoDate(), cfg->getLogStdFile(), cfg->getLogErrFile(), cfg->getLogNumLines(),
      cfg->getLogNumRotatedFiles());
   this->log = new LogContext("App");

   this->workQueue = new MultiWorkQueue();

   initLocalNodeInfo();

   registerSignalHandler();

   initStorage(); // (helperd actually does not use any storage in the sense of metadata etc)

   this->netMessageFactory = new NetMessageFactory();
}

void App::initLocalNodeInfo()
{
   bool useSDP = cfg->getConnUseSDP();

   StringList allowedInterfaces;
   std::string interfacesFilename = cfg->getConnInterfacesFile();
   if(interfacesFilename.length() )
      cfg->loadStringListFile(interfacesFilename.c_str(), allowedInterfaces);

   NetworkInterfaceCard::findAllInterfaces(allowedInterfaces, useSDP, localNicList);

   if(localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   localNicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);

   std::string nodeID = System::getHostname();
   localNode = std::make_shared<LocalNode>(nodeID, NumNodeID(1), 0, 0, localNicList);

   localNode->setNodeType(NODETYPE_Helperd);
   localNode->setFhgfsVersion(BEEGFS_VERSION_CODE);
}

void App::initStorage()
{
   /* note: helperd doesn't use any storage for metadata etc, so we just change the working
      directory to "/" here */

   std::string newWorkingDir("/");

   int changeDirRes = chdir(newWorkingDir.c_str() );
   if(changeDirRes)
   { // unable to change working directory
      throw InvalidConfigException("Unable to change working directory to: " + newWorkingDir +
         "(SysErr: " + System::getErrString() + ")");
   }
}

void App::initComponents()
{
   log->log(Log_DEBUG, "Initializing components...");

   unsigned short listenPort = cfg->getConnHelperdPortTCP();

   this->streamListener = new StreamListener(localNicList, workQueue, listenPort);

   workersInit();

   log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   this->streamListener->start();

   workersStart();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   workersStop();

   if(streamListener)
      streamListener->selfTerminate();

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

   const auto componentName = PThread::getCurrentThreadName();

   log.logErr(
         "The component [" + componentName + "] encountered an unrecoverable error. " +
         std::string("[SysErr: ") + System::getErrString() + "] " +
         std::string("Exception message: ") + e.what() );

   log.log(2, "Shutting down...");

   stopComponents();
}


void App::joinComponents()
{
   log->log(4, "Joining component threads...");

   /* (note: we need one thread for which we do an untimed join, so this should be a quite reliably
      terminating thread) */
   this->streamListener->join();

   workersJoin();
}

void App::workersInit()
{
   unsigned numWorkers = cfg->getTuneNumWorkers();

   for(unsigned i=0; i < numWorkers; i++)
   {
      Worker* worker = new Worker(
         std::string("Worker") + StringTk::intToStr(i+1), workQueue, QueueWorkType_INDIRECT);

      worker->setBufLens(APP_WORKERS_BUF_SIZE, APP_WORKERS_BUF_SIZE);

      workerList.push_back(worker);
   }

   for(unsigned i=0; i < APP_WORKERS_DIRECT_NUM; i++)
   {
      Worker* worker = new Worker(
         std::string("DirectWorker") + StringTk::intToStr(i+1), workQueue, QueueWorkType_DIRECT);

      worker->setBufLens(APP_WORKERS_BUF_SIZE, APP_WORKERS_BUF_SIZE);

      workerList.push_back(worker);
   }
}

void App::workersStart()
{
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      (*iter)->start();
   }
}

void App::workersStop()
{
   // need two loops because we don't know if the worker that handles the work will be the same that
   // received the self-terminate-request
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      (*iter)->selfTerminate();
   }

   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      workQueue->addDirectWork(new DummyWork() );
   }
}

void App::workersDelete()
{
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      delete(*iter);
   }

   workerList.clear();
}

void App::workersJoin()
{
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      waitForComponentTermination(*iter);
   }
}

void App::logInfos()
{
   // print software version (BEEGFS_VERSION)
   log->log(Log_CRITICAL, std::string("BeeGFS Helper Daemon Version: ") + BEEGFS_VERSION);

   // print debug version info
   LOG_DEBUG_CONTEXT(*this->log, 1, "--DEBUG VERSION--");

   // print local nodeID
   //log->log(Log_DEBUG, std::string("LocalNode: ") + localNode->getTypedNodeID() );

   log->log(Log_CRITICAL, "Client log messages will be prefixed with an asterisk (*) symbol.");


   // list usable network interfaces
   std::string nicListStr;
   std::string extendedNicListStr;
   for(NicAddressListIter nicIter = localNicList.begin(); nicIter != localNicList.end(); nicIter++)
   {
      std::string nicTypeStr;

      if(nicIter->nicType == NICADDRTYPE_RDMA)
         nicTypeStr = "RDMA";
      else
      if(nicIter->nicType == NICADDRTYPE_SDP)
         nicTypeStr = "SDP";
      else
      if(nicIter->nicType == NICADDRTYPE_STANDARD)
         nicTypeStr = "TCP";
      else
         nicTypeStr = "Unknown";

      nicListStr += std::string(nicIter->name) + "(" + nicTypeStr + ")" + " ";

      extendedNicListStr += "\n+ ";
      extendedNicListStr += NetworkInterfaceCard::nicAddrToString(&(*nicIter) ) + " ";
   }

   this->log->log(3, std::string("Usable NICs: ") + nicListStr);
   this->log->log(4, std::string("Extended list of usable NICs: ") + extendedNicListStr);

}

void App::daemonize()
{
   int nochdir = 1; // 1 to keep working directory
   int noclose = 0; // 1 to keep stdin/-out/-err open

   this->log->log(4, std::string("Detaching process...") );

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
         log->log(1, logContext, "Received a SIGINT. Shutting down...");
      } break;

      case SIGTERM:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         log->log(1, logContext, "Received a SIGTERM. Shutting down...");
      } break;

      default:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         log->log(1, logContext, "Received an unknown signal. Shutting down...");
      } break;
   }

   app->stopComponents();
}

