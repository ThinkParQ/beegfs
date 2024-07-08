#include <common/components/RegistrationDatagramListener.h>
#include <program/Program.h>

#include <toolkit/FsckTkEx.h>

#include <csignal>
#include <syslog.h>

#define APP_WORKERS_DIRECT_NUM   1
#define APP_SYSLOG_IDENTIFIER    "beegfs-fsck"

App::App(int argc, char** argv)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;

   this->cfg = NULL;
   this->netFilter = NULL;
   this->tcpOnlyFilter = NULL;
   this->log = NULL;
   this->mgmtNodes = NULL;
   this->metaNodes = NULL;
   this->storageNodes = NULL;
   this->internodeSyncer = NULL;
   this->targetMapper = NULL;
   this->targetStateStore = NULL;
   this->buddyGroupMapper = NULL;
   this->metaBuddyGroupMapper = NULL;
   this->workQueue = NULL;
   this->ackStore = NULL;
   this->netMessageFactory = NULL;
   this->dgramListener = NULL;
   this->modificationEventHandler = NULL;
   this->runMode = NULL;

}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   workersDelete();

   SAFE_DELETE(this->dgramListener);
   SAFE_DELETE(this->netMessageFactory);
   SAFE_DELETE(this->ackStore);
   SAFE_DELETE(this->workQueue);
   SAFE_DELETE(this->storageNodes);
   SAFE_DELETE(this->metaNodes);
   SAFE_DELETE(this->mgmtNodes);
   this->localNode.reset();
   SAFE_DELETE(this->buddyGroupMapper);
   SAFE_DELETE(this->metaBuddyGroupMapper);
   SAFE_DELETE(this->targetStateStore);
   SAFE_DELETE(this->targetMapper);
   SAFE_DELETE(this->internodeSyncer);
   SAFE_DELETE(this->log);
   SAFE_DELETE(this->tcpOnlyFilter);
   SAFE_DELETE(this->netFilter);
   SAFE_DELETE(this->cfg);
   SAFE_DELETE(this->runMode);

   Logger::destroyLogger();
   closelog();
}

void App::run()
{
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
      std::cerr << std::endl;

      if(this->log)
         log->logErr(e.what() );

      this->appResult = APPCODE_INVALID_CONFIG;
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
   bool componentsStarted = false;

   auto runModeEnum = Program::getApp()->getConfig()->determineRunMode();

   if (runModeEnum == RunMode_CHECKFS || runModeEnum == RunMode_ENABLEQUOTA)
   {
      // check if running as root
      if(geteuid())
      {
         std::cerr << std::endl 
               << "Running beegfs-fsck requires root privileges." 
               << std::endl << std::endl << std::endl;
         ModeHelp().execute();
         appResult = APPCODE_INITIALIZATION_ERROR;
         return;
      }

      if (runModeEnum == RunMode_CHECKFS)
         runMode = new ModeCheckFS();
      else if (runModeEnum == RunMode_ENABLEQUOTA)
         runMode = new ModeEnableQuota();
   }
   else if (runModeEnum == RunMode_HELP)
   {
      appResult = ModeHelp().execute();
      return;
   }
   else
   {
      ModeHelp().execute();
      appResult = APPCODE_INVALID_CONFIG;
      return;
   }

   initDataObjects(argc, argv);

   // wait for mgmtd
   if ( !cfg->getSysMgmtdHost().length() )
      throw InvalidConfigException("Management host undefined");

   bool mgmtWaitRes = waitForMgmtNode();
   if(!mgmtWaitRes)
   { // typically user just pressed ctrl+c in this case
      log->logErr("Waiting for beegfs-mgmtd canceled");
      appResult = APPCODE_RUNTIME_ERROR;
      return;
   }

   // init components
   try
   {
      initComponents();
   }
   catch (ComponentInitException& e)
   {
      FsckTkEx::fsckOutput(e.what(), OutputOptions_DOUBLELINEBREAK | OutputOptions_STDERR);

      FsckTkEx::fsckOutput("A hard error occurred. BeeGFS Fsck will abort.",
         OutputOptions_LINEBREAK | OutputOptions_STDERR);
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }

   // log system and configuration info

   logInfos();

   // start component threads
   startComponents();
   componentsStarted = true;

   try
   {
      appResult = runMode->execute();
   }
   catch (InvalidConfigException& e)
   {
      ModeHelp modeHelp;
      modeHelp.execute();

      appResult = APPCODE_INVALID_CONFIG;
   }
   catch (std::exception &e)
   {
      FsckTkEx::fsckOutput("Unrecoverable error. BeeGFS Fsck will abort.",
         OutputOptions_LINEBREAK | OutputOptions_ADDLINEBREAKBEFORE | OutputOptions_STDERR);
      FsckTkEx::fsckOutput(e.what(), OutputOptions_LINEBREAK | OutputOptions_STDERR);
   }

   // self-termination
   if(componentsStarted)
   {
      stopComponents();
      joinComponents();
   }
}

void App::initDataObjects(int argc, char** argv)
{
   this->netFilter = new NetFilter(cfg->getConnNetFilterFile() );
   this->tcpOnlyFilter = new NetFilter(cfg->getConnTcpOnlyFilterFile() );

   Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogNoDate(),
         cfg->getLogStdFile(), cfg->getLogNumLines(), cfg->getLogNumRotatedFiles());

   this->log = new LogContext("App");

   std::string interfacesFilename = this->cfg->getConnInterfacesFile();
   if ( interfacesFilename.length() )
      Config::loadStringListFile(interfacesFilename.c_str(), this->allowedInterfaces);

   this->targetMapper = new TargetMapper();
   this->targetStateStore = new TargetStateStore(NODETYPE_Storage);
   this->buddyGroupMapper = new MirrorBuddyGroupMapper(targetMapper);
   this->metaBuddyGroupMapper = new MirrorBuddyGroupMapper();

   this->mgmtNodes = new NodeStoreServers(NODETYPE_Mgmt, false);
   this->metaNodes = new NodeStoreServers(NODETYPE_Meta, false);
   this->storageNodes = new NodeStoreServers(NODETYPE_Storage, false);

   this->workQueue = new MultiWorkQueue();
   this->ackStore = new AcknowledgmentStore();

   initLocalNodeInfo();

   registerSignalHandler();

   this->netMessageFactory = new NetMessageFactory();
}

void App::findAllowedRDMAInterfaces(NicAddressList& outList) const
{
   Config* cfg = this->getConfig();

   if(cfg->getConnUseRDMA() && RDMASocket::rdmaDevicesExist() )
   {
      bool foundRdmaInterfaces = NetworkInterfaceCard::checkAndAddRdmaCapability(outList);
      if (foundRdmaInterfaces)
         outList.sort(NetworkInterfaceCard::NicAddrComp{&allowedInterfaces}); // re-sort the niclist
   }
}

void App::findAllowedInterfaces(NicAddressList& outList) const
{
   // discover local NICs and filter them
   NetworkInterfaceCard::findAllInterfaces(allowedInterfaces, outList);
   outList.sort(NetworkInterfaceCard::NicAddrComp{&allowedInterfaces});
}

void App::initLocalNodeInfo()
{

   findAllowedInterfaces(localNicList);
   findAllowedRDMAInterfaces(localNicList);

   if ( this->localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   initRoutingTable();
   updateRoutingTable();

   std::string nodeID = System::getHostname();

   this->localNode = std::make_shared<LocalNode>(NODETYPE_Client, nodeID, NumNodeID(), 0, 0,
         this->localNicList);
}

void App::initComponents()
{
   this->log->log(Log_DEBUG, "Initializing components...");

   // Note: We choose a random udp port here to avoid conflicts with the client
   unsigned short udpListenPort = 0;

   this->dgramListener = new DatagramListener(netFilter, localNicList, ackStore, udpListenPort,
      this->cfg->getConnRestrictOutboundInterfaces());

   // update the local node info with udp port
   this->localNode->updateInterfaces(dgramListener->getUDPPort(), 0, this->localNicList);

   this->internodeSyncer = new InternodeSyncer();

   workersInit();

   this->log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_SPAM, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   dgramListener->start();

   internodeSyncer->start();
   internodeSyncer->waitForServers();

   workersStart();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)
   workersStop();

   if(this->internodeSyncer)
      this->internodeSyncer->selfTerminate();

   if ( dgramListener )
   {
      dgramListener->selfTerminate();
      dgramListener->sendDummyToSelfUDP();
   }

   this->selfTerminate();
}

void App::joinComponents()
{
   log->log(4, "Joining component threads...");

   this->internodeSyncer->join();

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
   for ( WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++ )
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
   NicAddressList nicList(localNode->getNicList());
   logUsableNICs(log, nicList);

   // print net filters
   if ( netFilter->getNumFilterEntries() )
   {
      this->log->log(2,
         std::string("Net filters: ") + StringTk::uintToStr(netFilter->getNumFilterEntries()));
   }

   if(tcpOnlyFilter->getNumFilterEntries() )
   {
      this->log->log(Log_WARNING, std::string("TCP-only filters: ") +
         StringTk::uintToStr(tcpOnlyFilter->getNumFilterEntries() ) );
   }
}

/**
 * Request mgmt heartbeat and wait for the mgmt node to appear in nodestore.
 *
 * @return true if mgmt heartbeat received, false on error or thread selftermination order
 */
bool App::waitForMgmtNode()
{
   const unsigned waitTimeoutMS = 0; // infinite wait
   const unsigned nameResolutionRetries = 3;

   // choose a random udp port here
   unsigned udpListenPort = 0;
   unsigned udpMgmtdPort = cfg->getConnMgmtdPortUDP();
   std::string mgmtdHost = cfg->getSysMgmtdHost();

   RegistrationDatagramListener regDGramLis(this->netFilter, this->localNicList, this->ackStore,
      udpListenPort, this->cfg->getConnRestrictOutboundInterfaces());

   regDGramLis.start();

   log->log(Log_CRITICAL, "Waiting for beegfs-mgmtd@" +
      mgmtdHost + ":" + StringTk::uintToStr(udpMgmtdPort) + "...");

   bool gotMgmtd = NodesTk::waitForMgmtHeartbeat(
         this, &regDGramLis, this->mgmtNodes, mgmtdHost, udpMgmtdPort, waitTimeoutMS,
         nameResolutionRetries);

   regDGramLis.selfTerminate();
   regDGramLis.sendDummyToSelfUDP(); // for faster termination

   regDGramLis.join();

   return gotMgmtd;
}

void App::updateLocalNicList(NicAddressList& localNicList)
{
   std::vector<AbstractNodeStore*> allNodes({ mgmtNodes, metaNodes, storageNodes});
   updateLocalNicListAndRoutes(log, localNicList, allNodes);
   localNode->updateInterfaces(0, 0, localNicList);
   dgramListener->setLocalNicList(localNicList);
}

/*
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

   shallAbort.set(1);
   stopComponents();
}

void App::handleNetworkInterfaceFailure(const std::string& devname)
{
   LOG(GENERAL, ERR, "Network interface failure.",
      ("Device", devname));
   internodeSyncer->setForceCheckNetwork();
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
      }
         break;

      case SIGTERM:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         log->log(1, logContext, "Received a SIGTERM. Shutting down...");
      }
         break;

      default:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         log->log(1, logContext, "Received an unknown signal. Shutting down...");
      }
         break;
   }

   app->abort();
}

void App::abort()
{
   shallAbort.set(1);
   stopComponents();
}
