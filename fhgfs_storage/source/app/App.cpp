#include <common/app/log/LogContext.h>
#include <common/components/worker/queue/UserWorkContainer.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/ComponentInitException.h>
#include <common/components/RegistrationDatagramListener.h>
#include <common/net/message/nodes/RegisterNodeMsg.h>
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include <common/net/message/nodes/RegisterTargetMsg.h>
#include <common/net/message/nodes/RegisterTargetRespMsg.h>
#include <common/net/sock/RDMASocket.h>
#include <common/nodes/LocalNode.h>
#include <common/nodes/NodeFeatureFlags.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <components/streamlistenerv2/StorageStreamListenerV2.h>
#include <opentk/logging/SyslogLogger.h>
#include <program/Program.h>
#include <testing/StartupTests.h>
#include <testing/ComponentTests.h>
#include <testing/IntegrationTests.h>
#include <toolkit/StorageTkEx.h>
#include "App.h"

#include <signal.h>
#include <sys/resource.h>
#include <dlfcn.h>



#define APP_WORKERS_DIRECT_NUM   1
#define APP_SYSLOG_IDENTIFIER    "beegfs-storage"

#define APP_STORAGE_UMASK (0) // allow any creat() / mkdir() mode without masking anything

#define APP_LIB_ZFS_NAME "libzfs.so"


/**
 * Array of the feature bit numbers that are supported by this node/service.
 */
unsigned const APP_FEATURES[] =
{
   STORAGE_FEATURE_DUMMY,
};


App::App(int argc, char** argv)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;
   this->pidFileLockFD = -1;

   this->cfg = NULL;
   this->storageTargets = NULL;
   this->netFilter = NULL;
   this->tcpOnlyFilter = NULL;
   this->logger = NULL;
   this->log = NULL;
   this->allowedInterfaces = NULL;
   this->mgmtNodes = NULL;
   this->metaNodes = NULL;
   this->storageNodes = NULL;
   this->targetMapper = NULL;
   this->mirrorBuddyGroupMapper = NULL;
   this->targetStateStore = NULL;
   this->ackStore = NULL;
   this->sessions = NULL;
   this->chunkDirStore = NULL;
   this->nodeOperationStats = NULL;
   this->netMessageFactory = NULL;
   this->syncedStoragePaths = NULL;
   this->storageBenchOperator = NULL;

   this->chunkFetcher = NULL;

   this->dgramListener = NULL;
   this->connAcceptor = NULL;
   this->statsCollector = NULL;
   this->internodeSyncer = NULL;
   this->timerQueue = NULL;

   this->testRunner = NULL;

   this->nextNumaBindTarget = 0;

   this->workersRunning = false;

   this->exceededQuotaStore = NULL;
   this->buddyResyncer = NULL;
   this->chunkLockStore = NULL;

   this->dlOpenHandleLibZfs = NULL;
   this->libZfsErrorReported = false;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   workersDelete();

   SAFE_DELETE(this->timerQueue);
   SAFE_DELETE(this->internodeSyncer);
   SAFE_DELETE(this->statsCollector);
   SAFE_DELETE(this->connAcceptor);

   streamListenersDelete();

   SAFE_DELETE(this->buddyResyncer);
   SAFE_DELETE(this->chunkFetcher);
   SAFE_DELETE(this->dgramListener);
   SAFE_DELETE(this->allowedInterfaces);
   SAFE_DELETE(this->chunkDirStore);
   SAFE_DELETE(this->syncedStoragePaths);
   SAFE_DELETE(this->netMessageFactory);
   SAFE_DELETE(this->nodeOperationStats);
   SAFE_DELETE(this->sessions);
   SAFE_DELETE(this->ackStore);

   for(MultiWorkQueueMapIter iter = workQueueMap.begin(); iter != workQueueMap.end(); iter++)
      delete(iter->second);

   SAFE_DELETE(this->storageNodes);
   SAFE_DELETE(this->metaNodes);
   SAFE_DELETE(this->mgmtNodes);
   this->localNode.reset();
   SAFE_DELETE(this->mirrorBuddyGroupMapper);
   SAFE_DELETE(this->targetMapper);
   SAFE_DELETE(this->targetStateStore);
   SAFE_DELETE(this->log);
   SAFE_DELETE(this->logger);
   SAFE_DELETE(this->tcpOnlyFilter);
   SAFE_DELETE(this->netFilter);
   SAFE_DELETE(this->storageTargets); /* implicitly unlocks storage dirs */
   SAFE_DELETE(this->storageBenchOperator);
   SAFE_DELETE(this->testRunner);
   SAFE_DELETE(this->exceededQuotaStore);
   SAFE_DELETE(this->chunkLockStore);


   unlockAndDeletePIDFile(pidFileLockFD); // ignored if fd is -1

   SAFE_DELETE(this->cfg);

   SyslogLogger::destroyOnce();
}

void App::run()
{
   try
   {
      SyslogLogger::initOnce(APP_SYSLOG_IDENTIFIER);

      this->cfg = new Config(argc, argv);

      if ( this->cfg->getRunUnitTests() )
         runUnitTests();
      else
         runNormal();
   }
   catch (InvalidConfigException& e)
   {
      std::cerr << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
      std::cerr << std::endl;
      std::cerr << "[BeeGFS Storage Node Version: " << BEEGFS_VERSION << std::endl;
      std::cerr << "Refer to the default config file (/etc/beegfs/beegfs-storage.conf)" << std::endl;
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
 * @throw InvalidConfigException
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


   // init basic data objects & storage
   std::string localNodeID;
   NumNodeID localNodeNumID;

   preinitStorage(); // locks target dirs => call before anything else that accesses the disk

   initLogging();
   initLocalNodeIDs(localNodeID, localNodeNumID);
   initDataObjects();
   initBasicNetwork();

   initStorage();

   registerSignalHandler();


   bool startupTestsRes = runStartupTests();
   if(!startupTestsRes)
      return;

   // prepare ibverbs for forking
   RDMASocket::rdmaForkInitOnce();

   // detach process
   if(cfg->getRunDaemonized() )
      daemonize();

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

   // wait for management node heartbeat (required for localNodeNumID and target pre-registration)
   bool mgmtWaitRes = waitForMgmtNode();
   if(!mgmtWaitRes)
   { // typically user just pressed ctrl+c in this case
      log->logErr("Waiting for beegfs-mgmtd canceled");
      appResult = APPCODE_RUNTIME_ERROR;
      return;
   }

   // retrieve localNodeNumID from management node (if we don't have it yet)

   if(!localNodeNumID)
   { // no local num ID yet => try to retrieve one from mgmt
      bool preregisterNodeRes = preregisterNode(localNodeID, localNodeNumID);
      if(!preregisterNodeRes)
         throw InvalidConfigException("Node pre-registration at management node canceled");
   }

   if(!localNodeNumID) // just a sanity check that should never fail
      throw InvalidConfigException("Failed to retrieve numeric local node ID from mgmt");


   bool preregisterTargetsRes = preregisterTargets(localNodeNumID);
   if(!preregisterTargetsRes)
      throw InvalidConfigException("Target pre-registration at management node failed");

   initPostTargetRegistration();

   // we have all local node data now => init localNode

   initLocalNode(localNodeID, localNodeNumID);
   initLocalNodeNumIDFile(localNodeNumID);

   // log system and configuration info

   logInfos();

   bool downloadRes = registerAndDownloadMgmtInfo();
   if(!downloadRes)
   {
      log->log(1, "Downloading target states from management node failed. Shutting down...");
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }

   // init components

   try
   {
      initComponents();

      bool componentsTestsRes = runComponentTests();
      if(!componentsTestsRes)
         return;
   }
   catch(ComponentInitException& e)
   {
      log->logErr(e.what() );
      log->log(1, "A hard error occurred. Shutting down...");
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }

   // note: storage nodes & mappings must be downloaded before session restore (needed for mirrors)
   restoreSessions();

   // start component threads and join them

   startComponents();

   bool integrationTestsRes = runIntegrationTests();
   if(!integrationTestsRes)
      return;

   // session restore is finished so delete old session files
   // clean shutdown will generate a new session file
   deleteSessionFiles();

   // wait for termination
   joinComponents();

   // clean shutdown (at least no cache loss) => generate a new session file
   if(sessions)
      storeSessions();

   // close all client sessions
   InternodeSyncer::syncClientSessions({});


   log->log(Log_CRITICAL, "All components stopped. Exiting now!");
}

void App::runUnitTests()
{
   // if unit tests are configured to run, start the testrunner
   TestRunnerOutputFormat testRunnerOutputFormat;

   if(cfg->getTestOutputFormat() == "xml")
      testRunnerOutputFormat = TestRunnerOutputFormat_XML;
   else
      testRunnerOutputFormat = TestRunnerOutputFormat_TEXT;

   this->testRunner = new TestRunner(cfg->getTestOutputFile(), testRunnerOutputFormat);

   this->testRunner->start();

   // wait for it to finish
   this->testRunner->join();
}

/**
 * @return sets appResult on error, returns false on error, true otherwise (also if tests were
 * not configured to run)
 */
bool App::runStartupTests()
{
   if(!cfg->getDebugRunStartupTests() )
      return true;

   // run startup tests

   bool testRes = StartupTests::perform();
   if(!testRes)
   {
      log->log(Log_CRITICAL, "Startup Tests failed => shutting down...");

      appResult = APPCODE_RUNTIME_ERROR;
      return false;
   }

   return true;
}

/**
 * @return sets appResult on error, returns false on error, true otherwise (also if tests were
 * not configured to run)
 */
bool App::runComponentTests()
{
   if(!cfg->getDebugRunComponentTests() )
      return true;

   // run component tests

   bool testRes = ComponentTests::perform();
   if(!testRes)
   {
      log->log(Log_CRITICAL, "Component Tests failed => shutting down...");

      appResult = APPCODE_RUNTIME_ERROR;
      return false;
   }

   return true;
}

/**
 * @return sets appResult on error, returns false on error, true otherwise (also if tests were
 * not configured to run)
 */
bool App::runIntegrationTests()
{
   if(!cfg->getDebugRunIntegrationTests() )
      return true;

   // run integration tests

   bool testRes = IntegrationTests::perform();
   if(!testRes)
   {
      log->log(Log_CRITICAL, "Integration Tests failed => shutting down...");

      appResult = APPCODE_RUNTIME_ERROR;
      return false;
   }

   return true;
}

void App::initLogging() throw(InvalidConfigException)
{
   // check absolute log path to avoid chdir() problems
   Path logStdPath(cfg->getLogStdFile() );
   Path logErrPath(cfg->getLogErrFile() );

   if( (!logStdPath.empty() && !logStdPath.absolute() ) ||
       (!logErrPath.empty() && !logErrPath.absolute() ) )
   {
      throw InvalidConfigException("Path to log file must be absolute");
   }

   this->logger = new Logger(cfg);
   this->log = new LogContext("App");

}

/**
 * Init basic shared objects like work queues, node stores etc.
 */
void App::initDataObjects() throw(InvalidConfigException)
{
   this->mgmtNodes = new NodeStoreServersEx(NODETYPE_Mgmt);
   this->storageNodes = new NodeStoreServersEx(NODETYPE_Storage);

   this->targetMapper = new TargetMapper();
   this->storageNodes->attachTargetMapper(targetMapper);

   this->mirrorBuddyGroupMapper = new MirrorBuddyGroupMapper(targetMapper);

   this->targetStateStore = new TargetStateStore();
   this->targetMapper->attachStateStore(targetStateStore);

   this->metaNodes = new NodeStoreEx(NODETYPE_Meta);

   this->ackStore = new AcknowledgmentStore();

   this->sessions = new SessionStore();

   this->nodeOperationStats = new StorageNodeOpStats();

   this->syncedStoragePaths = new SyncedStoragePaths();

   this->chunkDirStore = new ChunkStore();

   this->exceededQuotaStore = new ExceededQuotaStore();

   this->chunkLockStore = new ChunkLockStore();
}

/**
 * Init basic networking data structures.
 *
 * Note: no RDMA is detected here, because this needs to be done later
 */
void App::initBasicNetwork()
{
   // check if management host is defined
   if(!cfg->getSysMgmtdHost().length() )
      throw InvalidConfigException("Management host undefined");

   // prepare filter for outgoing packets/connections
   this->netFilter = new NetFilter(cfg->getConnNetFilterFile() );
   this->tcpOnlyFilter = new NetFilter(cfg->getConnTcpOnlyFilterFile() );

   // prepare filter for interfaces
   StringList allowedInterfaces;
   std::string interfacesList = cfg->getConnInterfacesList();
   if(!interfacesList.empty() )
   {
      log->log(Log_DEBUG, "Allowed interfaces: " + interfacesList);
      StringTk::explodeEx(interfacesList, ',', true, &allowedInterfaces);
   }

   // discover local NICs and filter them
   NetworkInterfaceCard::findAllInterfaces(allowedInterfaces, cfg->getConnUseSDP(), localNicList);

   if(localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   localNicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);

   // prepare factory for incoming messages
   this->netMessageFactory = new NetMessageFactory();
}

/**
 * Load node IDs from disk or generate string ID.
 */
void App::initLocalNodeIDs(std::string& outLocalNodeID, NumNodeID& outLocalNodeNumID)
{
   // load nodeID file (from primary storage dir)

   Path storagePath(storageTargets->getPrimaryPath());
   Path nodeIDPath = storagePath / STORAGETK_NODEID_FILENAME;
   StringList nodeIDList; // actually, the file would contain only a single line

   bool idPathExists = StorageTk::pathExists(nodeIDPath.str() );
   if(idPathExists)
      ICommonConfig::loadStringListFile(nodeIDPath.str().c_str(), nodeIDList);

   if(!nodeIDList.empty() )
      outLocalNodeID = *nodeIDList.begin();

   // auto-generate nodeID if it wasn't loaded

   if(outLocalNodeID.empty() )
      outLocalNodeID = System::getHostname();

   // check targets for nodeID changes

   StringList targetPaths;

   storageTargets->getRawTargetPathsList(&targetPaths);

   for(StringListIter iter = targetPaths.begin(); iter != targetPaths.end(); iter++)
      StorageTk::checkOrCreateOrigNodeIDFile(*iter, outLocalNodeID);

   // load nodeNumID file (from primary storage dir)

   StorageTk::readNumIDFile(storageTargets->getPrimaryPath(), STORAGETK_NODENUMID_FILENAME,
      &outLocalNodeNumID);

   // note: localNodeNumID is still 0 here if it wasn't loaded from the file
}


/**
 * create and attach the localNode object.
 */
void App::initLocalNode(std::string& localNodeID, NumNodeID localNodeNumID)
{
   unsigned portUDP = cfg->getConnStoragePortUDP();
   unsigned portTCP = cfg->getConnStoragePortTCP();

   // create localNode object
   this->localNode = std::make_shared<LocalNode>(localNodeID, localNodeNumID, portUDP, portTCP,
         localNicList);

   localNode->setNodeType(NODETYPE_Storage);
   localNode->setFhgfsVersion(BEEGFS_VERSION_CODE);

   // nodeFeatureFlags
   BitStore nodeFeatureFlags;

   featuresToBitStore(APP_FEATURES, APP_FEATURES_ARRAY_LEN, &nodeFeatureFlags);
   localNode->setFeatureFlags(&nodeFeatureFlags);

   // attach to storageNodes store
   storageNodes->setLocalNode(this->localNode);
}

/**
 * Store numID file in each of the storage directories
 */
void App::initLocalNodeNumIDFile(NumNodeID localNodeNumID)
{
   StringList targetPaths;

   storageTargets->getRawTargetPathsList(&targetPaths);

   // walk all storage dirs and create numID file in each of them
   for(StringListIter iter = targetPaths.begin(); iter != targetPaths.end(); iter++)
   {
      StorageTk::createNumIDFile(*iter, STORAGETK_NODENUMID_FILENAME, localNodeNumID.val());
   }

}

/**
 * this contains things that would actually live inside initStorage() but need to be
 * done at an earlier stage (like working dir locking before log file creation).
 *
 * note: keep in mind that we don't have the logger here yet, because logging can only be
 * initialized after the working dir has been locked within this method.
 */
void App::preinitStorage() throw(InvalidConfigException)
{
   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   this->storageTargets = new StorageTargets(cfg); // implicitly locks storage dirs
}

void App::initStorage() throw(InvalidConfigException)
{
   setUmask();

   // change working dir (got no better place to go, so we change to root dir)
   const char* chdirPath = "/";

   int changeDirRes = chdir(chdirPath);
   if(changeDirRes)
   { // unable to change working directory
      throw InvalidConfigException(std::string("Unable to change working directory to: ") +
         chdirPath + "(SysErr: " + System::getErrString() + ")");
   }

   // storage target dirs (create subdirs, storage format file etc)
   storageTargets->prepareTargetDirs(cfg);

   // raise file descriptor limit
   if(cfg->getTuneProcessFDLimit() )
   {
      uint64_t oldLimit;

      bool setFDLimitRes = System::incProcessFDLimit(cfg->getTuneProcessFDLimit(), &oldLimit);
      if(!setFDLimitRes)
         log->log(Log_CRITICAL, std::string("Unable to increase process resource limit for "
            "number of file handles. Proceeding with default limit: ") +
            StringTk::uintToStr(oldLimit) + " " +
            "(SysErr: " + System::getErrString() + ")");
   }

}

/**
 * Remaining initialization of common objects that can only happen after the local target info is
 * complete (i.e. after preregisterTargets() ).
 */
void App::initPostTargetRegistration() throw(InvalidConfigException)
{
   /* init the quota block devices.
      we need the targetPaths for this, which are initialized in preregisterTargets() */

   this->storageTargets->initQuotaBlockDevices();

   /* init workQueueMap with one queue per targetID.
      requires targetIDs, so can only happen after preregisterTargets(). */

   UInt16List targetIDs;

   if(cfg->getTuneUsePerTargetWorkers() )
      this->storageTargets->getAllTargetIDs(&targetIDs);
   else
      targetIDs.push_back(0); // global worker set => create single targetID 0

   for(UInt16ListIter iter = targetIDs.begin(); iter != targetIDs.end(); iter++)
   {
      workQueueMap[*iter] = new MultiWorkQueue();

      if(cfg->getTuneUsePerUserMsgQueues() )
         workQueueMap[*iter]->setIndirectWorkList(new UserWorkContainer() );
   }
}

void App::initComponents() throw(ComponentInitException)
{
   this->log->log(Log_DEBUG, "Initializing components...");

   this->dgramListener = new DatagramListener(
      netFilter, localNicList, ackStore, cfg->getConnStoragePortUDP() );
   if(cfg->getTuneListenerPrioShift() )
      dgramListener->setPriorityShift(cfg->getTuneListenerPrioShift() );

   streamListenersInit();

   unsigned short listenPort = cfg->getConnStoragePortTCP();

   this->connAcceptor = new ConnAcceptor(this, localNicList, listenPort);

   this->statsCollector = new StorageStatsCollector(STATSCOLLECTOR_COLLECT_INTERVAL_MS,
      STATSCOLLECTOR_HISTORY_LENGTH);

   this->internodeSyncer = new InternodeSyncer();

   this->timerQueue = new TimerQueue(1, 1);

   this->storageBenchOperator = new StorageBenchOperator();

   this->chunkFetcher = new ChunkFetcher();

   this->buddyResyncer = new BuddyResyncer();

   workersInit();

   this->log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   this->dgramListener->start();

   streamListenersStart();

   this->connAcceptor->start();

   this->statsCollector->start();

   this->internodeSyncer->start();

   timerQueue->enqueue(std::chrono::seconds(30), InternodeSyncer::requestBuddyTargetStates);

   workersStart();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   workersStop();

   if (chunkFetcher)
      chunkFetcher->stopFetching(); // ignored if not running

   if(internodeSyncer)
      internodeSyncer->selfTerminate();

   if(statsCollector)
      statsCollector->selfTerminate();

   if(connAcceptor)
      connAcceptor->selfTerminate();

   streamListenersStop();

   if(dgramListener)
   {
      dgramListener->selfTerminate();
      dgramListener->sendDummyToSelfUDP(); // for faster termination
   }

   if(storageBenchOperator)
      storageBenchOperator->shutdownBenchmark();

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
      std::string("[SysErr: ") + System::getErrString() + "] " +
      std::string("Exception message: ") + e.what() );

   log.log(Log_WARNING, "Shutting down...");

   stopComponents();
}


void App::joinComponents()
{
   log->log(Log_DEBUG, "Joining component threads...");

   /* (note: we need one thread for which we do an untimed join, so this should be a quite reliably
      terminating thread) */
   this->statsCollector->join();

   workersJoin();

   // (the ChunkFetcher is not a normal component, so it gets special treatment here)
   if(chunkFetcher)
      chunkFetcher->waitForStopFetching();

   waitForComponentTermination(dgramListener);
   waitForComponentTermination(connAcceptor);

   streamListenersJoin();

   waitForComponentTermination(internodeSyncer);

   // (the StorageBenchOperator is not a normal component, so it gets special treatment here)
   if(storageBenchOperator)
      storageBenchOperator->waitForShutdownBenchmark();

   closeLibZfs();
}

void App::streamListenersInit() throw(ComponentInitException)
{
   this->numStreamListeners = cfg->getTuneNumStreamListeners();

   for(unsigned i=0; i < numStreamListeners; i++)
   {
      StreamListenerV2* listener = new StorageStreamListenerV2(
         std::string("StreamLis") + StringTk::uintToStr(i+1), this);

      if(cfg->getTuneListenerPrioShift() )
         listener->setPriorityShift(cfg->getTuneListenerPrioShift() );

      if(cfg->getTuneUseAggressiveStreamPoll() )
         listener->setUseAggressivePoll();

      streamLisVec.push_back(listener);
   }
}

void App::workersInit() throw(ComponentInitException)
{
   unsigned numWorkers = cfg->getTuneNumWorkers();

   unsigned currentTargetNum= 1; /* targetNum is only added to worker name if there are multiple
      target queues (i.e. workQueueMap.size > 1) */

   for(MultiWorkQueueMapIter iter = workQueueMap.begin(); iter != workQueueMap.end(); iter++)
   {
      for(unsigned i=0; i < numWorkers; i++)
      {
         Worker* worker = new Worker(
            std::string("Worker") + StringTk::uintToStr(i+1) +
            ( (workQueueMap.size() > 1) ? "-" + StringTk::uintToStr(currentTargetNum) : ""),
            iter->second, QueueWorkType_INDIRECT);

         worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

         workerList.push_back(worker);
      }

      for(unsigned i=0; i < APP_WORKERS_DIRECT_NUM; i++)
      {
         Worker* worker = new Worker(
            std::string("DirectWorker") + StringTk::uintToStr(i+1) +
            ( (workQueueMap.size() > 1) ? "-" + StringTk::uintToStr(currentTargetNum) : ""),
            iter->second, QueueWorkType_DIRECT);

         worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

         workerList.push_back(worker);
      }

      currentTargetNum++;
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

   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      if(cfg->getTuneWorkerNumaAffinity() )
         (*iter)->startOnNumaNode( (++nextNumaBindTarget) % numNumaNodes);
      else
         (*iter)->start();
   }

   SafeMutexLock lock(&this->mutexWorkersRunning); // L O C K
   this->workersRunning = true;
   lock.unlock(); // U N L O C K
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
   // need two loops because we don't know if the worker that handles the work will be the same that
   // received the self-terminate-request
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      (*iter)->selfTerminate();
   }

   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      (*iter)->getWorkQueue()->addDirectWork(new DummyWork() );
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
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
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
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      waitForComponentTermination(*iter);
   }

   SafeMutexLock lock(&this->mutexWorkersRunning); // L O C K
   this->workersRunning = false;
   lock.unlock(); // U N L O C K
}

void App::logInfos()
{
   // print software version (BEEGFS_VERSION)
   log->log(Log_CRITICAL, std::string("Version: ") + BEEGFS_VERSION);

   // print debug version info
   LOG_DEBUG_CONTEXT(*log, Log_CRITICAL, "--DEBUG VERSION--");

   // print local nodeIDs
   log->log(Log_WARNING, "LocalNode: " + localNode->getNodeIDWithTypeStr() );

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

   log->log(Log_WARNING, std::string("Usable NICs: ") + nicListStr);
   log->log(Log_DEBUG, std::string("Extended list of usable NICs: ") + extendedNicListStr);

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

   // storage tragets
   log->log(Log_WARNING, std::string("Storage targets: ") +
      StringTk::uintToStr(storageTargets->getNumTargets() ) );

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

void App::setUmask()
{
   ::umask(APP_STORAGE_UMASK);
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

   Logger* log = app->getLogger();
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

/**
 * Request mgmt heartbeat and wait for the mgmt node to appear in nodestore.
 *
 * @return true if mgmt heartbeat received, false on error or thread selftermination order
 */
bool App::waitForMgmtNode()
{
   int waitTimeoutMS = 0; // infinite wait

   unsigned udpListenPort = cfg->getConnStoragePortUDP();
   unsigned udpMgmtdPort = cfg->getConnMgmtdPortUDP();
   std::string mgmtdHost = cfg->getSysMgmtdHost();

   RegistrationDatagramListener regDGramLis(netFilter, localNicList, ackStore, udpListenPort);

   regDGramLis.start();

   log->log(Log_CRITICAL, "Waiting for beegfs-mgmtd@" +
      mgmtdHost + ":" + StringTk::uintToStr(udpMgmtdPort) + "...");


   bool gotMgmtd = NodesTk::waitForMgmtHeartbeat(
      this, &regDGramLis, mgmtNodes, mgmtdHost, udpMgmtdPort, waitTimeoutMS);

   regDGramLis.selfTerminate();
   regDGramLis.sendDummyToSelfUDP(); // for faster termination

   regDGramLis.join();

   return gotMgmtd;
}

/**
 * Pre-register node to get a numeric ID from mgmt.
 * This will only do something if we don't have localNodeNumID yet.
 *
 * @return true if pre-registration successful and localNodeNumID set.
 */
bool App::preregisterNode(std::string& localNodeID, NumNodeID& outLocalNodeNumID)
{
   static bool registrationFailureLogged = false; // to avoid log spamming

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
   {
      log->logErr("Unexpected: No management node found in store during node pre-registration.");
      return false;
   }

   BitStore nodeFeatureFlags;
   featuresToBitStore(APP_FEATURES, APP_FEATURES_ARRAY_LEN, &nodeFeatureFlags);

   RegisterNodeMsg msg(localNodeID, outLocalNodeNumID, NODETYPE_Storage, &localNicList,
      &nodeFeatureFlags, cfg->getConnStoragePortUDP(), cfg->getConnStoragePortTCP() );
   msg.setFhgfsVersion(BEEGFS_VERSION_CODE);

   Time startTime;
   Time lastRetryTime;
   unsigned nextRetryDelayMS = 0;

   char* respBuf;
   NetMessage* respMsg;

   // wait for mgmt node to appear and periodically resend request
   /* note: we usually expect not to loop here, because we already waited for mgmtd in
      waitForMgmtNode(), so mgmt should respond immediately. */

   while(!outLocalNodeNumID && !getSelfTerminate() )
   {
      if(lastRetryTime.elapsedMS() < nextRetryDelayMS)
      { // wait some time between retries
         waitForSelfTerminateOrder(nextRetryDelayMS);
         if(getSelfTerminate() )
            break;
      }

      // time to (re-)send request

      bool commRes = MessagingTk::requestResponse(
         *mgmtNode, &msg, NETMSGTYPE_RegisterNodeResp, &respBuf, &respMsg);

      if(commRes)
      { // communication successful
         RegisterNodeRespMsg* respMsgCast = (RegisterNodeRespMsg*)respMsg;

         outLocalNodeNumID = respMsgCast->getNodeNumID();

         if(!outLocalNodeNumID)
         { // mgmt rejected our registration
            log->logErr("ID reservation request was rejected by this mgmt node: " +
               mgmtNode->getTypedNodeID() );
         }
         else
            log->log(Log_WARNING, "Node ID reservation successful.");

         delete(respMsg);
         free(respBuf);

         break;
      }

      // comm failed => log status message

      if(!registrationFailureLogged)
      {
         log->log(Log_CRITICAL, "Node ID reservation failed. Management node offline? "
            "Will keep on trying...");
         registrationFailureLogged = true;
      }

      // calculate next retry wait time

      lastRetryTime.setToNow();
      nextRetryDelayMS = NodesTk::getRetryDelayMS(startTime.elapsedMS() );
   }

   return (outLocalNodeNumID != 0);
}

/**
 * Pre-register all currently unmapped targets to get a numeric ID from mgmt.
 *
 * @return true if pre-registration successful
 * @throw InvalidConfigException on target access error
 */
bool App::preregisterTargets(const NumNodeID localNodeNumID)
{
   bool retVal = true;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
   {
      log->logErr("Unexpected: No management node found in store during target pre-registration.");
      return false;
   }

   // validate IDs for mapped targets (i.e. targets that already have a numID)

   StringList targetPaths;

   storageTargets->getRawTargetPathsList(&targetPaths);

   for(StringListIter iter = targetPaths.begin(); iter != targetPaths.end(); iter++)
   {
      std::string targetID;
      uint16_t targetNumID;
      uint16_t newTargetNumID;

      // read or create target string ID file
      StorageTk::readOrCreateTargetIDFile(*iter, localNodeNumID, &targetID);

      // read target numeric ID file
      StorageTk::readNumIDFile(*iter, STORAGETK_TARGETNUMID_FILENAME, &targetNumID);

      // sanity check: make sure we don't have numID without stringID
      if(targetNumID && targetID.empty() )
      {
         log->logErr("Target sanity problem: "
            "Found targetNumID, but no corresponding string ID: " + *iter);

         retVal = false;
         break;
      }


      bool registerRes = preregisterTarget(*mgmtNode, targetID, targetNumID, &newTargetNumID);
      if(!registerRes)
      { // registration rejected
         retVal = false;
         break;
      }

      if(!targetNumID)
      { // got a new numID for this target
         log->log(Log_DEBUG, "Retrieved new numeric target ID: " +
            targetID + " -> " + StringTk::uintToStr(newTargetNumID) );

         StorageTk::createNumIDFile(*iter, STORAGETK_TARGETNUMID_FILENAME, newTargetNumID);
      }

      // map targetNumID/path pair
      bool addRes = storageTargets->addTargetDir(newTargetNumID, *iter);
      if (!addRes)
      { // adding the target failed
         retVal = false;
         break;
      }
   }

   return retVal;
}

/**
 * Pre-register target to get a numeric ID from mgmt (or to validate an existing numID).
 *
 * @param outNewTargetNumID the new targetNumID from mgmt if pre-registration was successful
 * @return true if pre-registration successful
 */
bool App::preregisterTarget(Node& mgmtNode, std::string targetID, uint16_t targetNumID,
   uint16_t* outNewTargetNumID)
{
   static bool registrationFailureLogged = false; // to avoid log spamming

   *outNewTargetNumID = 0; // "0" means undefined

   RegisterTargetMsg msg(targetID.c_str(), targetNumID);

   Time startTime;
   Time lastRetryTime;
   unsigned nextRetryDelayMS = 0;

   char* respBuf;
   NetMessage* respMsg;

   // wait for mgmt node response and periodically resend request
   /* note: we usually expect not to loop here, because we already waited for mgmtd in
      waitForMgmtNode(), so mgmt should respond immediately. */

   while(!getSelfTerminate() )
   {
      if(lastRetryTime.elapsedMS() < nextRetryDelayMS)
      { // wait some time between retries
         waitForSelfTerminateOrder(nextRetryDelayMS);
         if(getSelfTerminate() )
            break;
      }

      // time to (re-)send request

      bool commRes = MessagingTk::requestResponse(
         mgmtNode, &msg, NETMSGTYPE_RegisterTargetResp, &respBuf, &respMsg);

      if(commRes)
      { // communication successful
         RegisterTargetRespMsg* respMsgCast = (RegisterTargetRespMsg*)respMsg;

         *outNewTargetNumID = respMsgCast->getTargetNumID();

         if(!*outNewTargetNumID)
         { // mgmt rejected target registration
            log->logErr("Target ID reservation request was rejected by this mgmt node: " +
               mgmtNode.getTypedNodeID() );
         }
         else
            log->log(Log_DEBUG, "Target ID reservation successful.");

         delete(respMsg);
         free(respBuf);

         break;
      }

      // comm failed => log status message

      if(!registrationFailureLogged)
      {
         log->log(Log_CRITICAL, "Target ID reservation failed. Management node offline? "
            "Will keep on trying...");
         registrationFailureLogged = true;
      }

      // calculate next retry wait time

      lastRetryTime.setToNow();
      nextRetryDelayMS = NodesTk::getRetryDelayMS(startTime.elapsedMS() );
   }


   return (*outNewTargetNumID != 0);
}

/*
 * register ourself, our targets and download all nodes, mappings etc from mgmt.
 *
 * @param false if interrupted before download completed.
 */
bool App::registerAndDownloadMgmtInfo()
{
   Config* cfg = this->getConfig();

   int retrySleepTimeMS = 10000; // 10sec

   unsigned udpListenPort = cfg->getConnStoragePortUDP();
   bool allSuccessful = false;

   // start temporary registration datagram listener

   RegistrationDatagramListener regDGramLis(netFilter, localNicList, ackStore, udpListenPort);

   regDGramLis.start();


   // loop until we're registered and everything is downloaded (or until we got interrupted)

   do
   {
      // register ourself
      // (note: node registration needs to be done before downloads to get notified of updates)

      if(!InternodeSyncer::registerNode(&regDGramLis) ||
         !InternodeSyncer::registerTargetMappings() )
         continue;

      // download all mgmt info

      if(!InternodeSyncer::downloadAndSyncNodes() ||
         !InternodeSyncer::downloadAndSyncTargetMappings() ||
         !InternodeSyncer::downloadAndSyncMirrorBuddyGroups() )
         continue;

      UInt16List targetIDs;
      UInt8List reachabilityStates;
      UInt8List consistencyStates;
      if (!InternodeSyncer::downloadAndSyncTargetStates(
            targetIDs, reachabilityStates, consistencyStates) )
         continue;

      TargetStateMap statesFromMgmtd;
      StorageTargets::fillTargetStateMap(targetIDs, reachabilityStates, consistencyStates,
         statesFromMgmtd);

      TargetStateMap localChangedStates;
      storageTargets->decideResync(statesFromMgmtd, localChangedStates);

      StorageTargets::updateTargetStateLists(localChangedStates, targetIDs, reachabilityStates,
         consistencyStates);

      targetStateStore->syncStatesFromLists(targetIDs, reachabilityStates, consistencyStates);

      // If a local primary target needs a resync, wait for poffline timeout.
      storageTargets->addStartupTimeout();

      if(!InternodeSyncer::downloadAllExceededQuotaLists() )
         continue;

      // all done

      allSuccessful = true;
      break;

   } while(!waitForSelfTerminateOrder(retrySleepTimeMS) );


   // stop temporary registration datagram listener

   regDGramLis.selfTerminate();
   regDGramLis.sendDummyToSelfUDP(); // for faster termination

   regDGramLis.join();

   if(allSuccessful)
      log->log(Log_NOTICE, "Registration and management info download complete.");


   return allSuccessful;
}

bool App::restoreSessions()
{
   bool retVal = true;

   UInt16List allTargetIDs;
   this->storageTargets->getAllTargetIDs(&allTargetIDs);

   for(UInt16ListIter iter = allTargetIDs.begin(); iter != allTargetIDs.end(); iter++)
   {
      std::string path;
      if(this->storageTargets->getPath(*iter, &path) )
      {
         path += "/" + std::string(STORAGETK_SESSIONS_BACKUP_FILE_NAME);

         bool pathRes = StorageTk::pathExists(path);
         if(!pathRes)
            continue;

         bool loadRes = this->sessions->loadFromFile(path, *iter);
         if(!loadRes)
         {
            this->log->logErr("Could not restore all sessions from file " + path + " ; targetID: " +
               StringTk::uintToStr(*iter) );
            retVal = false;
         }
      }
      else
      { // should actually never happen
         this->log->logErr("No path available for targetID " + StringTk::uintToStr(*iter) );
         retVal = false;
      }
   }

   this->log->log(Log_NOTICE, StringTk::uintToStr(this->sessions->getSize() ) +
      " sessions restored.");

   return retVal;
}

bool App::storeSessions()
{
   bool retVal = true;

   UInt16List allTargetIDs;
   this->storageTargets->getAllTargetIDs(&allTargetIDs);

   for(UInt16ListIter iter = allTargetIDs.begin(); iter != allTargetIDs.end(); iter++)
   {
      std::string path;
      if(this->storageTargets->getPath(*iter, &path) )
      {
         path += "/" + std::string(STORAGETK_SESSIONS_BACKUP_FILE_NAME);

         bool pathRes = StorageTk::pathExists(path);
         if(pathRes)
            this->log->log(Log_WARNING, "Overwriting existing session file: " + path);

         bool saveRes = this->sessions->saveToFile(path, *iter);
         if(!saveRes)
         {
            this->log->logErr("Could not store all sessions to file " + path + "; "
               "targetID: " + StringTk::uintToStr(*iter) );
            retVal = false;
         }
      }
      else
      { // should actually never happens
         this->log->logErr("No path available for targetID " + StringTk::uintToStr(*iter) );
         retVal = false;
      }
   }

   if(retVal)
   {
      this->log->log(Log_NOTICE, StringTk::uintToStr(this->sessions->getSize() ) +
            " sessions stored.");
   }

   return retVal;
}

bool App::deleteSessionFiles()
{
   bool retVal = true;

   UInt16List allTargetIDs;
   this->storageTargets->getAllTargetIDs(&allTargetIDs);

   for(UInt16ListIter iter = allTargetIDs.begin(); iter != allTargetIDs.end(); iter++)
   {
      std::string path;
      if(this->storageTargets->getPath(*iter, &path) )
      {
         path += "/" + std::string(STORAGETK_SESSIONS_BACKUP_FILE_NAME);

         bool pathRes = StorageTk::pathExists(path);
         if(!pathRes)
            continue;

         if(remove(path.c_str() ) )
         {
            this->log->logErr("Could not remove session file " + path + " ; targetID: "
               + StringTk::uintToStr(*iter) + "; SysErr: " + System::getErrString() );
            retVal = false;
         }
      }
      else
      { // should actually never happens
         this->log->logErr("No path available for targetID " + StringTk::uintToStr(*iter) );
         retVal = false;
      }
   }

   return retVal;
}

bool App::openLibZfs()
{
   if(!dlOpenHandleLibZfs)
   {
      dlOpenHandleLibZfs = dlopen(APP_LIB_ZFS_NAME, RTLD_LAZY);

      if(!dlOpenHandleLibZfs)
      {
         log->log(Log_ERR, "Error during open of libzfs.so. Error: " + std::string(dlerror() ) );
         libZfsErrorReported = true;

         return false;
      }
   }

   return true;
}

bool App::closeLibZfs()
{
   if(dlOpenHandleLibZfs)
   {
      if(dlclose(dlOpenHandleLibZfs) )
      {
         log->log(Log_ERR, "Error during close of libzfs.so. Error: " + std::string(dlerror() ) );
         libZfsErrorReported = true;
         return false;
      }
   }

   return true;
}
