#include <common/app/log/LogContext.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/ComponentInitException.h>
#include <common/net/sock/RDMASocket.h>
#include <common/nodes/LocalNode.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/TimeAbs.h>
#include <modes/modehelpers/ModeInterruptedException.h>
#include <modes/ModeHelp.h>
#include <program/Program.h>
#include "App.h"

#include <csignal>
#include <syslog.h>

#define APP_WORKERS_DIRECT_NUM   1
#define APP_SYSLOG_IDENTIFIER    "beegfs-ctl"


App::App(int argc, char** argv)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;

   this->cfg = NULL;
   this->netFilter = NULL;
   this->tcpOnlyFilter = NULL;
   this->log = NULL;
   this->allowedInterfaces = NULL;
   this->mgmtNodes = NULL;
   this->metaNodes = NULL;
   this->storageNodes = NULL;
   this->clientNodes = NULL;
   this->targetMapper = NULL;
   this->mirrorBuddyGroupMapper = NULL;
   this->metaMirrorBuddyGroupMapper = NULL;
   this->targetStateStore = NULL;
   this->metaTargetStateStore = NULL;
   this->workQueue = NULL;
   this->ackStore = NULL;
   this->netMessageFactory = NULL;

   this->dgramListener = NULL;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   workersDelete();

   SAFE_DELETE(dgramListener);
   SAFE_DELETE(allowedInterfaces);
   SAFE_DELETE(netMessageFactory);
   SAFE_DELETE(ackStore);
   SAFE_DELETE(workQueue);
   SAFE_DELETE(clientNodes);
   SAFE_DELETE(storageNodes);
   SAFE_DELETE(metaNodes);
   SAFE_DELETE(mgmtNodes);
   this->localNode.reset();
   SAFE_DELETE(mirrorBuddyGroupMapper);
   SAFE_DELETE(metaMirrorBuddyGroupMapper);
   SAFE_DELETE(targetMapper);
   SAFE_DELETE(targetStateStore);
   SAFE_DELETE(metaTargetStateStore);
   SAFE_DELETE(log);
   SAFE_DELETE(tcpOnlyFilter);
   SAFE_DELETE(netFilter);
   SAFE_DELETE(cfg);

   Logger::destroyLogger();
   closelog();
}

void App::run()
{
   /* drop effective user and group ID, in case this executable has the setuid/setgid bit set
      (privileges will be re-elevated when necessary, e.g. for authenticat file reading) */
   System::dropUserAndGroupEffectiveID();

   try
   {
      openlog(APP_SYSLOG_IDENTIFIER, LOG_NDELAY | LOG_PID | LOG_CONS, LOG_DAEMON);

      this->cfg = new Config(argc, argv);

      runNormal();
   }
   catch (InvalidConfigException& e)
   {
      std::cerr << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
      std::cerr << std::endl;
      std::cerr << "[BeeGFS Control Tool Version: " << BEEGFS_VERSION << std::endl;
      std::cerr << "Refer to the default config file (/etc/beegfs/beegfs-client.conf)" << std::endl;
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
   const RunModesElem* runMode;

   // init data objects
   initDataObjects();

   // check if mgmt host is defined if mode is not "help"
   runMode = this->cfg->determineRunMode();
   if (runMode && runMode->needsCommunication && !cfg->getSysMgmtdHost().length())
      throw InvalidConfigException("Management host undefined");

   // init components

   try
   {
      initComponents();
   }
   catch(ComponentInitException& e)
   {
      log->logErr(e.what() );
      log->log(1, "A hard error occurred. Shutting down...");
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }


   // log system and configuration info

   logInfos();


   // detach process

   try
   {
      if(this->cfg->getRunDaemonized() )
         daemonize();
   }
   catch(InvalidConfigException& e)
   {
      log->logErr(e.what() );
      log->log(1, "A hard error occurred. Shutting down...");
      appResult = APPCODE_INVALID_CONFIG;
      return;
   }


   // start component threads

   startComponents();

   appResult = executeMode(runMode);

   // self-termination
   stopComponents();

   joinComponents();
   log->log(3, "All components stopped. Exiting now!");
}

int App::executeMode(const RunModesElem* runMode)
{
   try
   {
      if (runMode)
      {
         return runMode->instantiate()->execute();
      }
      else
      {  // execute modehelp, but return APPCODE_INVALID_CONFIG
         ModeHelp().execute();
         return APPCODE_INVALID_CONFIG;
      }
   }
   catch(ModeInterruptedException& e)
   {
      log->log(1, "Execution interrupted by signal!");
      return APPCODE_RUNTIME_ERROR;
   }
   catch(ComponentInitException& e)
   {
      std::cerr << e.what() << std::endl;
      log->log(1, e.what());
      return APPCODE_RUNTIME_ERROR;
   }
}

void App::initDataObjects()
{
   netFilter = new NetFilter(cfg->getConnNetFilterFile() );
   tcpOnlyFilter = new NetFilter(cfg->getConnTcpOnlyFilterFile() );

   // mute the standard logger if it has not been explicitly enabled
   Logger::createLogger(cfg->getLogEnabled() ? cfg->getLogLevel() : 0, cfg->getLogType(),
      cfg->getLogNoDate(), cfg->getLogStdFile(), cfg->getLogNumLines(),
      cfg->getLogNumRotatedFiles());

   log = new LogContext("App");

   allowedInterfaces = new StringList();
   std::string interfacesFilename = cfg->getConnInterfacesFile();
   if(interfacesFilename.length() )
      Config::loadStringListFile(interfacesFilename.c_str(), *allowedInterfaces);

   targetMapper = new TargetMapper();
   mirrorBuddyGroupMapper = new MirrorBuddyGroupMapper(targetMapper);
   metaMirrorBuddyGroupMapper = new MirrorBuddyGroupMapper();
   targetStateStore = new TargetStateStore(NODETYPE_Storage);
   metaTargetStateStore = new TargetStateStore(NODETYPE_Meta);
   storagePoolStore = boost::make_unique<StoragePoolStore>(mirrorBuddyGroupMapper,
                                                                 targetMapper);

   mgmtNodes = new NodeStoreServers(NODETYPE_Mgmt, false);
   metaNodes = new NodeStoreServers(NODETYPE_Meta, false);
   storageNodes = new NodeStoreServers(NODETYPE_Storage, false);
   clientNodes = new NodeStoreClients();
   workQueue = new MultiWorkQueue();
   ackStore = new AcknowledgmentStore();

   initLocalNodeInfo();

   registerSignalHandler();

   netMessageFactory = new NetMessageFactory();
}

void App::initLocalNodeInfo()
{
   bool useRDMA = this->cfg->getConnUseRDMA();

   NetworkInterfaceCard::findAll(allowedInterfaces, useRDMA, &localNicList);

   if(localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   localNicList.sort(NetworkInterfaceCard::NicAddrComp{allowedInterfaces});

   initRoutingTable();
   updateRoutingTable();

   std::string nodeID = System::getHostname() + "-" + StringTk::uint64ToHexStr(System::getPID() ) +
      "-" + StringTk::uintToHexStr(TimeAbs().getTimeMS() ) + "-" "ctl";

   localNode = std::make_shared<LocalNode>(NODETYPE_Client, nodeID, NumNodeID(), 0, 0,
         localNicList);
}

void App::initComponents()
{
   log->log(Log_DEBUG, "Initializing components...");

   // Note: We choose a random udp port here to avoid conflicts with the client
   unsigned short udpListenPort = 0; //this->cfg->getConnClientPortUDP();
   this->dgramListener = new DatagramListener(
      netFilter, localNicList, ackStore, udpListenPort,
      this->cfg->getConnRestrictOutboundInterfaces());
   this->dgramListener->setRecvTimeoutMS(20);

   workersInit();

   log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   this->dgramListener->start();

   workersStart();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   workersStop();

   if(dgramListener)
   {
      dgramListener->selfTerminate();
      dgramListener->sendDummyToSelfUDP(); // for faster termination
   }
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

/**
 * Handles a network interface failure.
 */
void App::handleNetworkInterfaceFailure(const std::string& devname)
{
   // Nothing to do. This App has no internodeSyncer that would rescan the
   // netdevs.
   LOG(GENERAL, ERR, "Network interface failure.",
      ("Device", devname));
}

void App::joinComponents()
{
   log->log(4, "Joining component threads...");

   /* (note: we need one thread for which we do an untimed join, so this should be a quite reliably
      terminating thread) */
   this->dgramListener->join();

   workersJoin();
}

void App::workersInit()
{
   unsigned numWorkers = cfg->getTuneNumWorkers();

   for(unsigned i=0; i < numWorkers; i++)
   {
      Worker* worker = new Worker(
         std::string("Worker") + StringTk::intToStr(i+1), workQueue, QueueWorkType_INDIRECT);
      workerList.push_back(worker);
   }

   for(unsigned i=0; i < APP_WORKERS_DIRECT_NUM; i++)
   {
      Worker* worker = new Worker(
         std::string("DirectWorker") + StringTk::intToStr(i+1), workQueue, QueueWorkType_DIRECT);
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
   log->log(Log_NOTICE, std::string("Version: ") + BEEGFS_VERSION);

   // print debug version info
   LOG_DEBUG_CONTEXT(*log, Log_CRITICAL, "--DEBUG VERSION--");

   // print local nodeID
   log->log(Log_NOTICE, std::string("LocalNode: ") + localNode->getTypedNodeID() );

   // list usable network interfaces
   NicAddressList nicList = getLocalNicList();
   logUsableNICs(log, nicList);

   // print net filters
   if(netFilter->getNumFilterEntries() )
   {
      log->log(Log_WARNING, std::string("Net filters: ") +
         StringTk::uintToStr(netFilter->getNumFilterEntries() ) );
   }

   if(tcpOnlyFilter->getNumFilterEntries() )
   {
      this->log->log(Log_WARNING, std::string("TCP-only filters: ") +
         StringTk::uintToStr(tcpOnlyFilter->getNumFilterEntries() ) );
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

