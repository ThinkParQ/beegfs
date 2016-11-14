#include <common/app/log/LogContext.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/ComponentInitException.h>
#include <common/components/RegistrationDatagramListener.h>
#include <common/net/message/nodes/RegisterNodeMsg.h>
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
//#include <common/net/sock/RDMASocket.h>
#include <common/nodes/LocalNode.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/NodesTk.h>
#include <program/Program.h>
#include <storage/MetadataEx.h>
//#include <testing/StartupTests.h>
//#include <testing/ComponentTests.h>
//#include <testing/IntegrationTests.h>
#include <toolkit/StorageTkEx.h>
#include <upgrade/Upgrade.h>

#include "App.h"

#include <signal.h>
#include <sys/resource.h>


#define APP_WORKERS_DIRECT_NUM   1


App::App(int argc, char** argv)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;
   this->pidFileLockFD = -1;
   this->workingDirLockFD = -1;

   this->cfg = NULL;
   this->netFilter = NULL;
   this->logger = NULL;
   this->log = NULL;
   this->allowedInterfaces = NULL;
   this->localNodeNumID = 0; // 0 means invalid/undefined
   this->localNode = NULL;
   this->mgmtNodes = NULL;
   this->metaNodes = NULL;
   this->storageNodes = NULL;
   this->clientNodes = NULL;
   this->metaCapacityPools = NULL;
   this->storageCapacityPools = NULL;
   this->targetMapper = NULL;
   this->workQueue = NULL;
   this->commSlaveQueue = NULL;
   this->disposalDir = NULL;
   this->rootDir = NULL;
   this->metaStore = NULL;
   this->ackStore = NULL;
   this->sessions = NULL;
   this->clientOperationStats = NULL;
   this->netMessageFactory = NULL;
   this->inodesPath = NULL;
   this->dentriesPath = NULL;

   this->dgramListener = NULL;
   this->heartbeatMgr = NULL;
//   this->streamListener = NULL;
   this->statsCollector = NULL;
   this->internodeSyncer = NULL;
   this->fullRefresher = NULL;

//   this->testRunner = NULL;

   this->nextNumaBindTarget = 0;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   commSlavesDelete();
   workersDelete();

   SAFE_DELETE(this->fullRefresher);
   SAFE_DELETE(this->internodeSyncer);
   SAFE_DELETE(this->statsCollector);
//   SAFE_DELETE(this->streamListener);
   SAFE_DELETE(this->heartbeatMgr);
   SAFE_DELETE(this->dgramListener);
   SAFE_DELETE(this->allowedInterfaces);
   SAFE_DELETE(this->dentriesPath);
   SAFE_DELETE(this->inodesPath);
   SAFE_DELETE(this->oldInodesPath);
   SAFE_DELETE(this->netMessageFactory);
   SAFE_DELETE(this->clientOperationStats);
   SAFE_DELETE(this->sessions);
   SAFE_DELETE(this->ackStore);
   if(this->disposalDir && this->metaStore)
      this->metaStore->releaseDir(this->disposalDir->getID() );
   if(this->rootDir && this->metaStore)
      this->metaStore->releaseDir(this->rootDir->getID() );
   SAFE_DELETE(this->metaStore);
   SAFE_DELETE(this->commSlaveQueue);
   SAFE_DELETE(this->workQueue);
   SAFE_DELETE(this->clientNodes);
   SAFE_DELETE(this->storageNodes);
   SAFE_DELETE(this->metaNodes);
   SAFE_DELETE(this->mgmtNodes);
   SAFE_DELETE(this->localNode);
   SAFE_DELETE(this->targetMapper);
   SAFE_DELETE(this->storageCapacityPools);
   SAFE_DELETE(this->metaCapacityPools);
   SAFE_DELETE(this->log);
   SAFE_DELETE(this->logger);
   SAFE_DELETE(this->netFilter);

//   SAFE_DELETE(this->testRunner);

   if(workingDirLockFD != -1)
      StorageTk::unlockWorkingDirectory(workingDirLockFD);

   unlockPIDFile(pidFileLockFD); // ignored if fd is -1

   SAFE_DELETE(this->cfg);
}

/**
 * Initialize config and run app either in normal mode or in special unit tests mode.
 */
void App::run()
{
   char* progPath = this->argv[0];
   std::string progName = basename(progPath);

   try
   {
      if (argc != 4 && argc != 5)
      {
         Upgrade::printUsage(progName);
         ::exit(1);
      }

      // Config must be initialized first
      this->cfg = new Config(argc, argv);

      std::string storeMetaDirectory = this->cfg->getStoreMetaDirectory();
      if (storeMetaDirectory.empty() )
      {
         std::cerr << "Missing argument: storeMetaDirectory! " << std::endl;
         Upgrade::printUsage(progName);
         ::exit(1);
      }

      std::string metaIdMapFile   = this->cfg->getMetaIdMapFile();
      if (metaIdMapFile.empty() )
      {
         std::cerr << "Missing argument: metaIdMapFile! " << std::endl;
         Upgrade::printUsage(progName);
         ::exit(1);
      }

      std::string targetIdMapFile = this->cfg->getTargetIdMapFile();
      if (targetIdMapFile.empty() )
      {
         std::cerr << "Missing argument: targetIdMapFile! " << std::endl;
         Upgrade::printUsage(progName);
         ::exit(1);
      }


      runNormal();
   }
   catch (InvalidConfigException& e)
   {
      std::cerr << std::endl;
      std::cerr << "Error: " << e.what() << std::endl;
      std::cerr << std::endl;

      if(this->log)
         log->logErr(e.what() );

      Upgrade::printUsage(progName);

      appResult = APPCODE_INVALID_CONFIG;
      return;
   }
}

/**
 * @throw InvalidConfigException on error
 */
void App::runNormal()
{
   this->metaStore = new MetaStore(); // requires initialized config

   // loads meta string to numeric ID map
   std::string metaIDMapFileName = getConfig()->getMetaIdMapFile();
   if (!Upgrade::loadIDMap(metaIDMapFileName, &this->metaIdMap) )
   {
      std::cerr << "Failed to load MetaIdMapFile. Aborting!" << std::endl;
      ::exit(1);
   }


   // init basic data objects & storage

   preinitStorage(); // locks working dir => call before anything else that accesses the disk

   initLogging();
   initLocalNodeIDs();
   // initDataObjects();
   // initNet();

   initStorage();
   // initRootDir();
   // initDisposalDir();

   // registerSignalHandler();


   // detach process

   // if(cfg->getRunDaemonized() )
   //   daemonize();

   // retrieve localNodeNumID from management node (if we don't have it yet)

#if 0
   if(!localNodeNumID)
   { // no local num ID yet => try to retrieve one from mgmt
      bool mgmtWaitRes = waitForMgmtNode();
      if(!mgmtWaitRes)
      { // typically user just pressed ctrl+c in this case
         log->logErr("Waiting for fhgfs-mgmtd canceled");
         appResult = APPCODE_RUNTIME_ERROR;
         return;
      }

      bool preregisterRes = preregisterNode();
      if(!preregisterRes)
         throw InvalidConfigException("Pre-registration at management node canceled");
   }

   if(!localNodeNumID) // just a sanity check that should never fail
      throw InvalidConfigException("Failed to retrieve numeric local node ID from mgmt");
#endif


   // we have all local node data now => init localNode

   initLocalNode();
   initLocalNodeNumIDFile();

#if 0
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
#endif

   // log system and configuration info

   logInfos();

   // start component threads and join them
   // startComponents();

   // wait for termination
   // joinComponents();

   // close all client sessions
   // NodeList emptyClientsList;
   // heartbeatMgr->syncClients(&emptyClientsList, false);


   // log->log(Log_CRITICAL, "All components stopped. Exiting now!");

   // now run our real goal - upgrade the data
   Upgrade upgrade;
   upgrade.runUpgrade();
}

#if 0
void App::runUnitTests()
{
   // if unit tests are configured to run, start the testrunner
   TestRunnerOutputFormat testRunnerOutputFormat;

   this->logger = new Logger(cfg);
   this->log = new LogContext("App");

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

#endif

void App::initLogging() throw(InvalidConfigException)
{
   // check absolute log path to avoid chdir() problems
   Path logStdPath(cfg->getLogStdFile() );
   Path logErrPath(cfg->getLogErrFile() );

   if( (!logStdPath.getIsEmpty() && !logStdPath.isAbsolute() ) ||
       (!logErrPath.getIsEmpty() && !logErrPath.isAbsolute() ) )
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
   this->metaNodes = new NodeStoreServersEx(NODETYPE_Meta);
   this->storageNodes = new NodeStoreServersEx(NODETYPE_Storage);
   this->clientNodes = new NodeStoreClientsEx();

   this->metaCapacityPools = new TargetCapacityPools(0, 0);
   this->metaNodes->attachCapacityPools(metaCapacityPools);

   this->targetMapper = new TargetMapper();
   this->storageNodes->attachTargetMapper(targetMapper);

   this->storageCapacityPools = new TargetCapacityPools(0, 0);
   this->targetMapper->attachCapacityPools(storageCapacityPools);

   this->workQueue = new MultiWorkQueue();
   this->commSlaveQueue = new MultiWorkQueue();

   this->ackStore = new AcknowledgmentStore();

   this->sessions = new SessionStore();

   this->clientOperationStats = new MetaNodeOpStats();
}

/**
 * Init basic networking data structures.
 */
void App::initNet() throw(InvalidConfigException)
{
   // check if management host is defined
   if(!cfg->getSysMgmtdHost().length() )
      throw InvalidConfigException("Management host undefined");

   // prepare filter for outgoing packets/connections
   this->netFilter = new NetFilter(cfg);

   // prepare filter for published local interfaces
   this->allowedInterfaces = new StringList();

   std::string interfacesList = cfg->getConnInterfacesList();
   if(!interfacesList.empty() )
   {
      log->log(Log_DEBUG, "Allowed interfaces: " + interfacesList);
      StringTk::explodeEx(interfacesList, ',', true, allowedInterfaces);
   }

   // prepare ibverbs
   // RDMASocket::rdmaForkInitOnce();

   // discover local NICs and filter them
   bool useSDP = cfg->getConnUseSDP();
   bool useRDMA = cfg->getConnUseRDMA();

   NetworkInterfaceCard::findAll(allowedInterfaces, useSDP, useRDMA, &localNicList);

   if(localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   localNicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);

   // prepare factory for incoming messages
   this->netMessageFactory = new NetMessageFactory();
}

/**
 * Load node IDs from disk or generate string ID.
 */
void App::initLocalNodeIDs() throw(InvalidConfigException)
{
   // load (or generate) nodeID and compare to original nodeID

   Path metaPath(metaPathStr);
   Path nodeIDPath(metaPath, STORAGETK_NODEID_FILENAME);
   StringList nodeIDList; // actually contains only a single line

   bool idPathExists = StorageTk::pathExists(nodeIDPath.getPathAsStr() );
   if(idPathExists)
      ICommonConfig::loadStringListFile(nodeIDPath.getPathAsStr().c_str(), nodeIDList);

   if(!nodeIDList.empty() )
      localNodeID = *nodeIDList.begin();

   // auto-generate nodeID if it wasn't loaded

   if(localNodeID.empty() )
      localNodeID = System::getHostname();

   // check for nodeID changes

   // load nodeNumID file
   // StorageTk::readNumIDFile(metaPath.getPathAsStr(), STORAGETK_NODENUMID_FILENAME, &localNodeNumID);

   // note: localNodeNumID is still 0 here if it wasn't loaded from the file
}

/**
 * create and attach the localNode object, store numID in storage dir
 */
void App::initLocalNode() throw(InvalidConfigException)
{
   // unsigned portUDP = cfg->getConnMetaPortUDP();
   unsigned portUDP = 0; // irrelevant for the upgrade tool

   // create localNode object
   this->localNode = new LocalNode(localNodeID, localNodeNumID, portUDP, localNicList);

   localNode->setNodeType(NODETYPE_Meta);
   localNode->setFhgfsVersion(FHGFS_VERSION_CODE);

   // attach to metaNodes store
   // metaNodes->setLocalNode(this->localNode);
}

/**
 * Store numID file in storage directory
 */
void App::initLocalNodeNumIDFile() throw(InvalidConfigException)
{
   StorageTk::createNumIDFile(metaPathStr, STORAGETK_NODENUMID_FILENAME, localNodeNumID);
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
   Path metaPath(cfg->getStoreMetaDirectory() );
   this->metaPathStr = metaPath.getPathAsStr(); // normalize

   if(metaPath.getIsEmpty() )
      throw InvalidConfigException("No metadata storage directory specified");

   if(!metaPath.isAbsolute() ) /* (check to avoid problems after chdir later) */
      throw InvalidConfigException("Path to storage directory must be absolute: " + metaPathStr);

   if(!StorageTk::checkStorageFormatFileExists(metaPathStr) )
      throw InvalidConfigException("Path does not have FhGFS meta file structure: " + metaPathStr);

   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   if(!StorageTk::createPathOnDisk(metaPath, false) )
      throw InvalidConfigException("Unable to create metadata directory: " + metaPathStr +
         " (" + System::getErrString(errno) + ")" );

   this->workingDirLockFD = StorageTk::lockWorkingDirectory(cfg->getStoreMetaDirectory() );
   if(workingDirLockFD == -1)
      throw InvalidConfigException("Unable to lock working directory: " + metaPathStr);
}

void App::initStorage() throw(InvalidConfigException)
{
   // change working dir to meta directory
   int changeDirRes = chdir(metaPathStr.c_str() );
   if(changeDirRes)
   { // unable to change working directory
      throw InvalidConfigException("Unable to change working directory to: " + metaPathStr + " "
         "(SysErr: " + System::getErrString() + ")");
   }

   StorageTkEx::checkStorageFormatFile(metaPathStr);

   // dentries directory
   dentriesPath = new Path(META_DENTRIES_SUBDIR_NAME);
   dentriesPath->initPathStr(); // to init internal path string (and enable later use with "const")
   StorageTk::initHashPaths(*dentriesPath,
      META_DENTRIES_LEVEL1_SUBDIR_NUM, META_DENTRIES_LEVEL2_SUBDIR_NUM);

   // inodes directory
   oldInodesPath = new Path(META_INODES_SUBDIR_NAME_OLD);
   oldInodesPath->initPathStr();
   inodesPath = new Path(META_INODES_SUBDIR_NAME);
   inodesPath->initPathStr(); // to init internal path string (and enable later use with "const")
   if(!StorageTk::createPathOnDisk(*this->inodesPath, false) )
      throw InvalidConfigException("Unable to create directory: " +  inodesPath->getPathAsStr() );
   StorageTk::initHashPaths(*inodesPath,
      META_INODES_LEVEL1_SUBDIR_NUM, META_INODES_LEVEL2_SUBDIR_NUM);

#if 0 // not needed for the upgrade tool
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
#endif

}


void App::initRootDir() throw(InvalidConfigException)
{
   // try to load root dir from disk (through metaStore) or create a new one

   this->rootDir = this->metaStore->referenceDir(META_ROOTDIR_ID_STR, true);
   if(this->rootDir)
   { // loading succeeded => init rootNodeID
      this->log->log(Log_NOTICE, "Root directory loaded.");

      uint16_t rootDirOwner = rootDir->getOwnerNodeID();

      // try to set rootDirOwner as root node
      if(rootDirOwner && metaNodes->setRootNodeNumID(rootDirOwner, false) )
      { // new root node accepted (check if rootNode is localNode)

         if(localNodeNumID == rootDirOwner)
            log->log(Log_CRITICAL, "I got root (by possession of root directory)");
         else
            log->log(Log_CRITICAL,
               "Root metadata server (by possession of root directory): " +
               StringTk::uintToStr(rootDirOwner) );
      }
   }
   else
   { // failed to load root directory => create a new rootDir
      this->log->log(Log_CRITICAL,
         "This appears to be a new storage directory. Creating a new root dir.");

      UInt16Vector stripeTargets;
      unsigned defaultChunkSize = this->cfg->getTuneDefaultChunkSize();
      unsigned defaultNumStripeTargets = this->cfg->getTuneDefaultNumStripeTargets();
      Raid0Pattern stripePattern(defaultChunkSize, stripeTargets, defaultNumStripeTargets);

      DirInode* newRootDir = new DirInode(META_ROOTDIR_ID_STR,
         S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO, 0, 0, 0, stripePattern);

      this->metaStore->makeDirInode(newRootDir, false);
      this->rootDir = this->metaStore->referenceDir(META_ROOTDIR_ID_STR, true);

      if(!this->rootDir)
      { // error
         this->log->logErr("Failed to store root directory. Unable to proceed.");
         throw InvalidConfigException("Failed to store root directory");
      }
   }

}

void App::initDisposalDir() throw(InvalidConfigException)
{
   // try to load disposal dir from disk (through metaStore) or create a new one

   this->disposalDir = this->metaStore->referenceDir(META_DISPOSALDIR_ID_STR, true);
   if(this->disposalDir)
   { // loading succeeded
      this->log->log(Log_DEBUG, "Disposal directory loaded.");
   }
   else
   { // failed to load disposal directory => create a new one
      this->log->log(Log_DEBUG, "Creating a new disposal directory.");

      UInt16Vector stripeTargets;
      unsigned defaultChunkSize = this->cfg->getTuneDefaultChunkSize();
      unsigned defaultNumStripeTargets = this->cfg->getTuneDefaultNumStripeTargets();
      Raid0Pattern stripePattern(defaultChunkSize, stripeTargets, defaultNumStripeTargets);

      DirInode* newDisposalDir = new DirInode(META_DISPOSALDIR_ID_STR,
         S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO, 0, 0, 0, stripePattern);

      this->metaStore->makeDirInode(newDisposalDir, false);
      this->disposalDir = newDisposalDir;

      if(!this->disposalDir)
      { // error
         this->log->logErr("Failed to store disposal directory. Unable to proceed.");
         throw InvalidConfigException("Failed to store disposal directory");
      }

   }

}

void App::initComponents() throw(ComponentInitException)
{
   this->log->log(Log_DEBUG, "Initializing components...");

   this->dgramListener = new DatagramListener(
      netFilter, localNicList, ackStore, cfg->getConnMetaPortUDP() );
   if(cfg->getTuneListenerPrioShift() )
      dgramListener->setPriorityShift(cfg->getTuneListenerPrioShift() );

   this->heartbeatMgr = new HeartbeatManager(dgramListener);

   // unsigned short listenPort = cfg->getConnMetaPortTCP();

#if 0
   this->streamListener = new StreamListener(localNicList, workQueue, listenPort);
   if(cfg->getTuneListenerPrioShift() )
      streamListener->setPriorityShift(cfg->getTuneListenerPrioShift() );
#endif

   this->statsCollector = new StatsCollector(workQueue, STATSCOLLECTOR_COLLECT_INTERVAL_MS,
      STATSCOLLECTOR_HISTORY_LENGTH);

   this->internodeSyncer = new InternodeSyncer();

   this->fullRefresher = new FullRefresher();

   workersInit();
   commSlavesInit();

   this->log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   this->dgramListener->start();
   this->heartbeatMgr->start();

#if 0
   if(!cfg->getTuneListenerNumaAffinity() )
      this->streamListener->start();
   else
      this->streamListener->startOnNumaNode( (++nextNumaBindTarget) % System::getNumNumaNodes() );
#endif

   this->statsCollector->start();

   this->internodeSyncer->start();

   workersStart();
   commSlavesStart();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   // note: no commslave stop here, because that would keep workers from terminating

   workersStop();

   if(fullRefresher)
      fullRefresher->stopRefreshing();

   if(internodeSyncer)
      internodeSyncer->selfTerminate();

   if(statsCollector)
      statsCollector->selfTerminate();

#if 0
   if(streamListener)
      streamListener->selfTerminate();
#endif

   if(dgramListener)
   {
      dgramListener->selfTerminate();
      dgramListener->sendDummyToSelfUDP(); // for faster termination
   }

   if(heartbeatMgr)
      heartbeatMgr->selfTerminate();

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

   // (the FullRefresher is not a normal component, so it gets special treatment here)
   if(fullRefresher)
      fullRefresher->waitForStopRefreshing();

   waitForComponentTermination(dgramListener);
   waitForComponentTermination(heartbeatMgr);
   // waitForComponentTermination(streamListener);
   waitForComponentTermination(internodeSyncer);

   commSlavesStop(); // placed here because otherwise it would keep workers from terminating
   commSlavesJoin();
}

void App::workersInit() throw(ComponentInitException)
{
   unsigned numWorkers = cfg->getTuneNumWorkers();

   for(unsigned i=0; i < numWorkers; i++)
   {
      Worker* worker = new Worker(
         std::string("Worker") + StringTk::intToStr(i+1), workQueue, QueueWorkType_INDIRECT);

      worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

      workerList.push_back(worker);
   }

   for(unsigned i=0; i < APP_WORKERS_DIRECT_NUM; i++)
   {
      Worker* worker = new Worker(
         std::string("DirectWorker") + StringTk::intToStr(i+1), workQueue, QueueWorkType_DIRECT);

      worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

      workerList.push_back(worker);
   }
}

void App::commSlavesInit() throw(ComponentInitException)
{
   unsigned numCommSlaves = cfg->getTuneNumCommSlaves();

   for(unsigned i=0; i < numCommSlaves; i++)
   {
      Worker* worker = new Worker(
         std::string("CommSlave") + StringTk::intToStr(i+1), commSlaveQueue, QueueWorkType_DIRECT);

      worker->setBufLens(cfg->getTuneCommSlaveBufSize(), cfg->getTuneCommSlaveBufSize() );

      commSlaveList.push_back(worker);
   }
}

void App::workersStart()
{
   unsigned numNumaNodes = System::getNumNumaNodes();

   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      if(!cfg->getTuneWorkerNumaAffinity() )
         (*iter)->start();
      else
         (*iter)->startOnNumaNode( (++nextNumaBindTarget) % numNumaNodes);
   }
}

void App::commSlavesStart()
{
   unsigned numNumaNodes = System::getNumNumaNodes();

   for(WorkerListIter iter = commSlaveList.begin(); iter != commSlaveList.end(); iter++)
   {
      if(!cfg->getTuneWorkerNumaAffinity() )
         (*iter)->start();
      else
         (*iter)->startOnNumaNode( (++nextNumaBindTarget) % numNumaNodes);
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

void App::commSlavesStop()
{
   // need two loops because we don't know if the worker that handles the work will be the same that
   // received the self-terminate-request
   for(WorkerListIter iter = commSlaveList.begin(); iter != commSlaveList.end(); iter++)
   {
      (*iter)->selfTerminate();
   }

   for(WorkerListIter iter = commSlaveList.begin(); iter != commSlaveList.end(); iter++)
   {
      commSlaveQueue->addDirectWork(new DummyWork() );
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

void App::commSlavesDelete()
{
   for(WorkerListIter iter = commSlaveList.begin(); iter != commSlaveList.end(); iter++)
   {
      delete(*iter);
   }

   commSlaveList.clear();
}

void App::workersJoin()
{
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      waitForComponentTermination(*iter);
   }
}

void App::commSlavesJoin()
{
   for(WorkerListIter iter = commSlaveList.begin(); iter != commSlaveList.end(); iter++)
   {
      waitForComponentTermination(*iter);
   }
}

void App::logInfos()
{
#if 0
   // list usable network interfaces
   NicAddressList nicList(localNode->getNicList() );

   std::string nicListStr;
   std::string extendedNicListStr;
   for(NicAddressListIter nicIter = nicList.begin(); nicIter != nicList.end(); nicIter++)
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

      extendedNicListStr += "\n" "+ ";
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

   // print numa info
   if(cfg->getTuneListenerNumaAffinity() || cfg->getTuneWorkerNumaAffinity() )
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
#endif
}

void App::daemonize() throw(InvalidConfigException)
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

/**
 * Request mgmt heartbeat and wait for the mgmt node to appear in nodestore.
 *
 * @return true if mgmt heartbeat received, false on error or thread selftermination order
 */
bool App::waitForMgmtNode()
{
   int waitTimeoutMS = 0; // infinite wait

   unsigned udpListenPort = cfg->getConnMetaPortUDP();
   unsigned udpMgmtdPort = cfg->getConnMgmtdPortUDP();
   std::string mgmtdHost = cfg->getSysMgmtdHost();

   RegistrationDatagramListener regDGramLis(netFilter, localNicList, ackStore, udpListenPort);

   regDGramLis.start();

   log->log(Log_CRITICAL, "Waiting for fhgfs-mgmtd: " +
      mgmtdHost + ":" + StringTk::uintToStr(udpMgmtdPort) );


   bool gotMgmtd = NodesTk::waitForMgmtHeartbeat(
      this, &regDGramLis, mgmtNodes, mgmtdHost, udpMgmtdPort, waitTimeoutMS);

   regDGramLis.selfTerminate();
   regDGramLis.sendDummyToSelfUDP(); // for faster termination

   regDGramLis.join();

   return gotMgmtd;
}

/**
 * Pre-register node to get a numeric ID from mgmt.
 *
 * @return true if pre-registration successful and localNodeNumID set.
 */
bool App::preregisterNode()
{
   static bool registrationFailureLogged = false; // to avoid log spamming

   Node* mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
   {
      log->logErr("Unexpected: No management node found in store during node pre-registration.");
      return false;
   }

   uint16_t rootNodeID = metaNodes->getRootNodeNumID();

   RegisterNodeMsg msg(localNodeID.c_str(), localNodeNumID, NODETYPE_Meta, rootNodeID,
      &localNicList, cfg->getConnMetaPortUDP(), cfg->getConnMetaPortTCP() );
   msg.setFhgfsVersion(FHGFS_VERSION_CODE);

   Time startTime;
   Time lastRetryTime;
   unsigned nextRetryDelayMS = 0;

   char* respBuf;
   NetMessage* respMsg;

   // wait for mgmt node to appear and periodically resend request
   /* note: we usually expect not to loop here, because we already waited for mgmtd in
      waitForMgmtNode(), so mgmt should respond immediately. */

   while(!localNodeNumID && !getSelfTerminate() )
   {
      if(lastRetryTime.elapsedMS() < nextRetryDelayMS)
      { // wait some time between retries
         waitForSelfTerminateOrder(nextRetryDelayMS);
         if(getSelfTerminate() )
            break;
      }

      // time to (re-)send request

      bool commRes = MessagingTk::requestResponse(
         mgmtNode, &msg, NETMSGTYPE_RegisterNodeResp, &respBuf, &respMsg);

      if(commRes)
      { // communication successful
         RegisterNodeRespMsg* respMsgCast = (RegisterNodeRespMsg*)respMsg;

         localNodeNumID = respMsgCast->getNodeNumID();

         if(!localNodeNumID)
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

   mgmtNodes->releaseNode(&mgmtNode);


   return (localNodeNumID != 0);
}

