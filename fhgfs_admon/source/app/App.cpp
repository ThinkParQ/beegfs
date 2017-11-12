#include <opentk/logging/SyslogLogger.h>
#include <program/Program.h>
#include "App.h"


#define APP_WORKERS_DIRECT_NUM   1
#define APP_WORKERS_BUF_SIZE     (1024*1024)
#define APP_SYSLOG_IDENTIFIER    "beegfs-admon"


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
   this->ackStore = NULL;
   this->netMessageFactory = NULL;
   this->datagramListener = NULL;
   this->internodeSyncer = NULL;
   this->storageNodes = NULL;
   this->metaNodes = NULL;
   this->mgmtNodes = NULL;
   this->clientNodes = NULL;
   this->targetMapper = NULL;
   this->metaBuddyGroupMapper = NULL;
   this->storageBuddyGroupMapper = NULL;
   this->webServer = NULL;
   this->db = NULL;
   this->nodeListRequestor = NULL;
   this->clientStatsOperator = NULL;
   this->dataRequestorIOStats = NULL;
   this->mailer = NULL;
   this->jobRunner = NULL;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   workersDelete();

   SAFE_DELETE(this->datagramListener);
   SAFE_DELETE(this->netMessageFactory);
   SAFE_DELETE(this->internodeSyncer);
   SAFE_DELETE(this->ackStore);
   SAFE_DELETE(this->workQueue);
   SAFE_DELETE(this->metaNodes);
   SAFE_DELETE(this->storageNodes);
   SAFE_DELETE(this->mgmtNodes);
   SAFE_DELETE(this->clientNodes);
   this->localNode.reset();
   SAFE_DELETE(this->targetMapper);
   SAFE_DELETE(this->metaBuddyGroupMapper);
   SAFE_DELETE(this->storageBuddyGroupMapper);
   SAFE_DELETE(this->webServer);
   SAFE_DELETE(this->db);
   SAFE_DELETE(this->nodeListRequestor);
   SAFE_DELETE(this->clientStatsOperator);
   SAFE_DELETE(this->dataRequestorIOStats);
   SAFE_DELETE(this->mailer);
   SAFE_DELETE(this->jobRunner);
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
   catch (DatabaseException &e)
   {
      std::cerr << "Could not initialize database. " << std::string(e.what()) << std::endl;

      log->logErr("Could not initialize database. " + std::string(e.what()) + ". Error during "
         "execution of: " + std::string(e.queryStr()) + " Execution ends with error code: " +
         StringTk::intToStr(e.errorCode()));

      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }
   catch (InvalidConfigException& e)
   {
      std::cerr << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
      std::cerr << std::endl;
      std::cerr << "[BeeGFS Admon Version: " << BEEGFS_VERSION << std::endl;
      std::cerr << "Refer to the default config file (/etc/beegfs/beegfs-admon.conf)" << std::endl;
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
   // init data objects
   initDataObjects(argc, argv);

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

   // prepare ibverbs for forking
   RDMASocket::rdmaForkInitOnce();

   // detach process
   try
   {
      if(this->cfg->getRunDaemonized() )
         daemonize();
   }
   catch(InvalidConfigException& e)
   {
      log->logErr(e.what() );
      log->log(Log_CRITICAL, "A hard error occurred. Shutting down...");
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

   logInfos();
   startComponents();
   joinComponents();

   log->log(Log_CRITICAL, "All components stopped. Exiting now!");
}

void App::initDataObjects(int argc, char** argv) throw(InvalidConfigException, DatabaseException)
{
   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   this->netFilter = new NetFilter(cfg->getConnNetFilterFile() );
   this->tcpOnlyFilter = new NetFilter(cfg->getConnTcpOnlyFilterFile() );

   Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogErrsToStdlog(), 
      cfg->getLogNoDate(), cfg->getLogStdFile(), cfg->getLogErrFile(), cfg->getLogNumLines(),
      cfg->getLogNumRotatedFiles());
   this->log = new LogContext("App");
   this->runtimeCfg = new RuntimeConfig(cfg);
   this->db = new Database();
   this->db->readRuntimeConfig();

   this->workQueue = new MultiWorkQueue();
   this->metaNodes = new NodeStoreMetaEx();
   this->storageNodes = new NodeStoreStorageEx();
   this->mgmtNodes = new NodeStoreMgmtEx();
   this->clientNodes = new NodeStoreClients(false);
   this->ackStore = new AcknowledgmentStore();

   this->targetMapper = new TargetMapper();
   this->storageNodes->attachTargetMapper(targetMapper);

   this->metaBuddyGroupMapper = new MirrorBuddyGroupMapper();
   this->storageBuddyGroupMapper = new MirrorBuddyGroupMapper();

   initLocalNodeInfo();

   registerSignalHandler();

   this->netMessageFactory = new NetMessageFactory();
}

void App::initLocalNodeInfo() throw(InvalidConfigException)
{
   bool useSDP = this->cfg->getConnUseSDP();
   unsigned portUDP = cfg->getConnAdmonPortUDP();

   StringList allowedInterfaces;
   std::string interfacesFilename = cfg->getConnInterfacesFile();
   if(interfacesFilename.length() )
      cfg->loadStringListFile(interfacesFilename.c_str(), allowedInterfaces);

   NetworkInterfaceCard::findAllInterfaces(allowedInterfaces, useSDP, localNicList);

   if(localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   localNicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);

   std::string nodeID = System::getHostname();

   localNode = std::make_shared<LocalNode>(nodeID, NumNodeID(1), portUDP, 0, localNicList);

   localNode->setFhgfsVersion(BEEGFS_VERSION_CODE);
}

void App::initComponents() throw(ComponentInitException)
{
   log->log(Log_DEBUG, "Initializing components...");

   int listenPort = cfg->getConnAdmonPortUDP();
   int httpPort = cfg->getHttpPort();
   this->webServer = new WebServer(httpPort);

   this->datagramListener = new DatagramListener(netFilter, localNicList, ackStore, listenPort);

   workersInit();

   this->internodeSyncer = new InternodeSyncer();
   this->nodeListRequestor = new NodeListRequestor();
   this->clientStatsOperator = new StatsOperator();
   this->dataRequestorIOStats = new DataRequestorIOStats(metaNodes, storageNodes, workQueue,
      cfg->getQueryInterval() );
   this->mailer = new Mailer();
   this->jobRunner = new ExternalJobRunner();

   log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   this->datagramListener->start();
   workersStart();
   this->nodeListRequestor->start();
   this->internodeSyncer->start();
   this->dataRequestorIOStats->start();
   this->webServer->start();
   this->mailer->start();
   this->jobRunner->start();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   workersStop();

   if(webServer)
      webServer->selfTerminate();

   if(nodeListRequestor)
      nodeListRequestor->selfTerminate();

   if(dataRequestorIOStats)
      dataRequestorIOStats->selfTerminate();

   if(datagramListener)
      datagramListener->selfTerminate();

   if(internodeSyncer)
      internodeSyncer->selfTerminate();

   if(mailer)
      mailer->selfTerminate();

   if(jobRunner)
      jobRunner->selfTerminate();

   if(clientStatsOperator)
      clientStatsOperator->shutdown();

   this->selfTerminate(); /* this flag can be noticed by thread-independent methods and is also
      required e.g. to let App::waitForMgmtNode() know that it should cancel */
}

/**
 * Handles expections that lead to the termination of a component.
 * Initiates an application shutdown.
 */
void App::handleComponentException(std::exception& e)
{
   const char* logContext = "App::handleComponentException";
   LogContext log(logContext);

   log.logErr(std::string("This component encountered an unrecoverable error. ") +
      std::string("[SysErrnoMessage: ") + System::getErrString() + "] " +
      std::string("Exception message: ") + e.what() );

   log.log(Log_WARNING, "Shutting down...");

   stopComponents();
}


void App::joinComponents()
{
   log->log(Log_DEBUG, "Joining component threads...");

   /* (note: we need one thread for which we do an untimed join, so this should be a quite reliably
      terminating thread) */
   this->datagramListener->join();

   workersJoin();

   waitForComponentTermination(nodeListRequestor);
   waitForComponentTermination(dataRequestorIOStats);
   waitForComponentTermination(webServer);
   waitForComponentTermination(mailer);
   waitForComponentTermination(jobRunner);
   waitForComponentTermination(internodeSyncer);

   // (the ClientStatsController is not a normal component, so it gets special treatment here)
   if(clientStatsOperator)
      clientStatsOperator->waitForShutdown();
}

void App::workersInit() throw(ComponentInitException)
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
   this->log->log(Log_CRITICAL, std::string("Version: ") + BEEGFS_VERSION);

   // print debug version info
   LOG_DEBUG_CONTEXT(*this->log, Log_CRITICAL, "--DEBUG VERSION--");

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

   this->log->log(Log_WARNING, std::string("Usable NICs: ") + nicListStr);
   this->log->log(Log_DEBUG, std::string("Extended List of usable NICs: ") + extendedNicListStr);

   // print net filters
   if(netFilter->getNumFilterEntries() )
   {
      this->log->log(Log_WARNING, std::string("Net filters: ") +
         StringTk::uintToStr(netFilter->getNumFilterEntries() ) );
   }

   if(tcpOnlyFilter->getNumFilterEntries() )
   {
      this->log->log(Log_WARNING, std::string("TCP-only filters: ") +
         StringTk::uintToStr(tcpOnlyFilter->getNumFilterEntries() ) );
   }
}

void App::daemonize() throw(InvalidConfigException)
{
   int nochdir = 1; // 1 to keep working directory
   int noclose = 0; // 1 to keep stdin/-out/-err open

   this->log->log(Log_DEBUG, std::string("Detaching process...") );

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
   //    we need something like a timed lock for the log mutex. if it succeeds withing a
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


void App::getStorageNodesAsStringList(StringList *outList)
{
   auto node = storageNodes->referenceFirstNode();
   while(node)
   {
      std::string nodeID = node->getID();
      outList->push_back(nodeID);

      node = storageNodes->referenceNextNode(node);
   }
}

void App::getMetaNodesAsStringList(StringList *outList)
{
   auto node = metaNodes->referenceFirstNode();
   while(node)
   {
      std::string nodeID = node->getID();
      outList->push_back(nodeID);

      node = metaNodes->referenceNextNode(node);
   }
}

void App::getStorageNodeNumIDs(NumNodeIDList *outList)
{
   auto node = storageNodes->referenceFirstNode();
   while(node)
   {
      NumNodeID nodeID = node->getNumID();
      outList->push_back(nodeID);

      node = storageNodes->referenceNextNode(node);
   }
}

void App::getMetaNodeNumIDs(NumNodeIDList *outList)
{
   auto node = metaNodes->referenceFirstNode();
   while(node)
   {
      NumNodeID nodeID = node->getNumID();
      outList->push_back(nodeID);

      node = metaNodes->referenceNextNode(node);
   }
}
