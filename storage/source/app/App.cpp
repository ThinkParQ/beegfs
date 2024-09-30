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
#include <common/nodes/TargetStateInfo.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/OfflineWaitTimeoutTk.h>
#include <common/toolkit/ZipIterator.h>
#include <components/streamlistenerv2/StorageStreamListenerV2.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include <boost/format.hpp>
#include "App.h"

#include <csignal>
#include <syslog.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <blkid/blkid.h>
#include <uuid/uuid.h>
#include <fstream>
#include <sstream>




#define APP_WORKERS_DIRECT_NUM   1
#define APP_SYSLOG_IDENTIFIER    "beegfs-storage"

#define APP_STORAGE_UMASK (0) // allow any creat() / mkdir() mode without masking anything

#define APP_LIB_ZFS_NAME "libzfs.so"

App::App(int argc, char** argv)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;

   this->cfg = NULL;
   this->storageTargets = NULL;
   this->netFilter = NULL;
   this->tcpOnlyFilter = NULL;
   this->log = NULL;
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
   this->timerQueue = new TimerQueue();

   this->nextNumaBindTarget = 0;

   this->workersRunning = false;

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

   SAFE_DELETE(this->internodeSyncer);
   SAFE_DELETE(this->statsCollector);
   SAFE_DELETE(this->connAcceptor);

   streamListenersDelete();

   SAFE_DELETE(this->buddyResyncer);
   SAFE_DELETE(this->chunkFetcher);
   SAFE_DELETE(this->dgramListener);
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
   SAFE_DELETE(this->tcpOnlyFilter);
   SAFE_DELETE(this->netFilter);
   SAFE_DELETE(this->storageTargets);
   SAFE_DELETE(this->storageBenchOperator);
   SAFE_DELETE(this->chunkLockStore);

   SAFE_DELETE(this->cfg);

   delete timerQueue;

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
   checkTargetsUUIDs();
   initLocalNodeIDs(localNodeID, localNodeNumID);
   initDataObjects();
   initBasicNetwork();

   initStorage();

   registerSignalHandler();

   // detach process
   if(cfg->getRunDaemonized() )
      daemonize();

   log->log(Log_NOTICE, "Built "
#ifdef BEEGFS_NVFS
      "with"
#else
      "without"
#endif
      " NVFS RDMA support.");

   // find RDMA interfaces (based on TCP/IP interfaces)

   // note: we do this here, because when we first create an RDMASocket (and this is done in this
   // check), the process opens the verbs device. Recent OFED versions have a check if the
   // credentials of the opening process match those of the calling process (not only the values
   // are compared, but the pointer is checked for equality). Thus, the first open needs to happen
   // after the fork, because we need to access the device in the child process.
   findAllowedRDMAInterfaces(localNicList);

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


   auto preregisterTargetsRes = preregisterTargets(localNodeNumID);
   if(!preregisterTargetsRes)
      throw InvalidConfigException("Target pre-registration at management node failed");

   storageTargets = new StorageTargets(std::move(*preregisterTargetsRes));

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

   // check and log if enterprise features are used
   checkEnterpriseFeatureUsage();

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

   // note: storage nodes & mappings must be downloaded before session restore (needed for mirrors)
   restoreSessions();

   // start component threads and join them

   startComponents();

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

void App::initLogging()
{
   // check absolute log path to avoid chdir() problems
   Path logStdPath(cfg->getLogStdFile() );

   if(!logStdPath.empty() && !logStdPath.absolute())
   {
      throw InvalidConfigException("Path to log file must be absolute");
   }

   Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogNoDate(),
         cfg->getLogStdFile(), cfg->getLogNumLines(), cfg->getLogNumRotatedFiles());
   this->log = new LogContext("App");
}

/**
 * Init basic shared objects like work queues, node stores etc.
 */
void App::initDataObjects()
{
   this->mgmtNodes = new NodeStoreServers(NODETYPE_Mgmt, true);
   this->storageNodes = new NodeStoreServers(NODETYPE_Storage, true);

   this->targetMapper = new TargetMapper();
   this->storageNodes->attachTargetMapper(targetMapper);

   this->mirrorBuddyGroupMapper = new MirrorBuddyGroupMapper(targetMapper);
   this->storagePoolStore = boost::make_unique<StoragePoolStore>(mirrorBuddyGroupMapper,
                                                                 targetMapper);
   this->targetStateStore = new TargetStateStore(NODETYPE_Storage);
   this->targetMapper->attachStateStore(targetStateStore);

   this->metaNodes = new NodeStoreServers(NODETYPE_Meta, true);

   this->ackStore = new AcknowledgmentStore();

   this->sessions = new SessionStore();

   this->nodeOperationStats = new StorageNodeOpStats();

   this->syncedStoragePaths = new SyncedStoragePaths();

   this->chunkDirStore = new ChunkStore();

   this->chunkLockStore = new ChunkLockStore();
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

   if(outList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   outList.sort(NetworkInterfaceCard::NicAddrComp{&allowedInterfaces});
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

   Config* cfg = this->getConfig();

   // prepare filter for interfaces
   std::string interfacesList = cfg->getConnInterfacesList();
   if(!interfacesList.empty() )
   {
      log->log(Log_DEBUG, "Allowed interfaces: " + interfacesList);
      StringTk::explodeEx(interfacesList, ',', true, &allowedInterfaces);
   }

   findAllowedInterfaces(localNicList);

   noDefaultRouteNets = std::make_shared<NetVector>();
   if(!initNoDefaultRouteList(noDefaultRouteNets.get()))
      throw InvalidConfigException("Failed to parse connNoDefaultRoute");

   initRoutingTable();
   updateRoutingTable();

   // prepare factory for incoming messages
   this->netMessageFactory = new NetMessageFactory();
}

/**
 * Load node IDs from disk or generate string ID.
 */
void App::initLocalNodeIDs(std::string& outLocalNodeID, NumNodeID& outLocalNodeNumID)
{
   // load nodeID file (from first storage dir)

   const auto& storagePath = cfg->getStorageDirectories().front();
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

   for (const auto& path : cfg->getStorageDirectories())
      StorageTk::checkOrCreateOrigNodeIDFile(path.str(), outLocalNodeID);

   // load nodeNumID file (from primary storage dir)

   StorageTk::readNumIDFile(storagePath.str(), STORAGETK_NODENUMID_FILENAME, &outLocalNodeNumID);

   // note: localNodeNumID is still 0 here if it wasn't loaded from the file
}


/**
 * create and attach the localNode object.
 */
void App::initLocalNode(std::string& localNodeID, NumNodeID localNodeNumID)
{
   unsigned portUDP = cfg->getConnStoragePortUDP();
   unsigned portTCP = cfg->getConnStoragePortTCP();
   NicAddressList nicList = getLocalNicList();

   // create localNode object
   this->localNode = std::make_shared<LocalNode>(NODETYPE_Storage, localNodeID, localNodeNumID,
      portUDP, portTCP, nicList);

   // attach to storageNodes store
   storageNodes->setLocalNode(this->localNode);
}

/**
 * Store numID file in each of the storage directories
 */
void App::initLocalNodeNumIDFile(NumNodeID localNodeNumID)
{
   for (const auto& path : cfg->getStorageDirectories())
      StorageTk::createNumIDFile(path.str(), STORAGETK_NODENUMID_FILENAME, localNodeNumID.val());
}

/**
 * this contains things that would actually live inside initStorage() but need to be
 * done at an earlier stage (like working dir locking before log file creation).
 *
 * note: keep in mind that we don't have the logger here yet, because logging can only be
 * initialized after the working dir has been locked within this method.
 */
void App::preinitStorage()
{
   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   if (cfg->getStorageDirectories().empty())
      throw InvalidConfigException("No storage target directories defined");

   for (const auto& path: cfg->getStorageDirectories())
   {
      if (!path.absolute()) /* (check to avoid problems after chdir) */
         throw InvalidConfigException("Path to storage target directory must be absolute: " +
            path.str());

      if (!cfg->getStoreAllowFirstRunInit() && !StorageTarget::isTargetDir(path))
         throw InvalidConfigException(std::string("Found uninitialized storage target directory "
            "and initialization has been disabled: ") + path.str());

      auto lockFD = StorageTk::lockWorkingDirectory(path.str());
      if (!lockFD.valid())
         throw InvalidConfigException("Invalid storage directory: locking failed");

      storageTargetLocks.push_back(std::move(lockFD));
   }
}

void App::initStorage()
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
   for (const auto& path : cfg->getStorageDirectories())
      StorageTarget::prepareTargetDir(path);

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
void App::initPostTargetRegistration()
{
   /* init workQueueMap with one queue per targetID.
      requires targetIDs, so can only happen after preregisterTargets(). */

   const auto addWQ = [&] (const auto& mapping) {
      workQueueMap[mapping.first] = new MultiWorkQueue();

      if (cfg->getTuneUsePerUserMsgQueues())
         workQueueMap[mapping.first]->setIndirectWorkList(new UserWorkContainer());
   };

   if (cfg->getTuneUsePerTargetWorkers())
      std::for_each(storageTargets->getTargets().begin(), storageTargets->getTargets().end(), addWQ);
   else
      addWQ(std::make_pair(0, nullptr)); // global worker set => create single targetID 0

   // init exceeded quota stores
   for (const auto& mapping : storageTargets->getTargets())
      exceededQuotaStores.add(mapping.first);
}

void App::initComponents()
{
   this->log->log(Log_DEBUG, "Initializing components...");
   NicAddressList nicList = getLocalNicList();
   this->dgramListener = new DatagramListener(
      netFilter, nicList, ackStore, cfg->getConnStoragePortUDP(),
      this->cfg->getConnRestrictOutboundInterfaces() );
   if(cfg->getTuneListenerPrioShift() )
      dgramListener->setPriorityShift(cfg->getTuneListenerPrioShift() );

   streamListenersInit();

   unsigned short listenPort = cfg->getConnStoragePortTCP();

   this->connAcceptor = new ConnAcceptor(this, nicList, listenPort);

   this->statsCollector = new StorageStatsCollector(STATSCOLLECTOR_COLLECT_INTERVAL_MS,
      STATSCOLLECTOR_HISTORY_LENGTH);

   this->internodeSyncer = new InternodeSyncer();

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

   timerQueue->start();

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

void App::updateLocalNicList(NicAddressList& localNicList)
{
   std::vector<AbstractNodeStore*> allNodes({ mgmtNodes, metaNodes, storageNodes});
   updateLocalNicListAndRoutes(log, localNicList, allNodes);
   localNode->updateInterfaces(0, 0, localNicList);
   dgramListener->setLocalNicList(localNicList);
   connAcceptor->updateLocalNicList(localNicList);
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

   log.log(Log_WARNING, "Shutting down...");

   stopComponents();
}

/**
 * Called when a network device failure has been detected.
 */
void App::handleNetworkInterfaceFailure(const std::string& devname)
{
   LOG(GENERAL, ERR, "Network interface failure.",
      ("Device", devname));
   internodeSyncer->setForceCheckNetwork();
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

void App::streamListenersInit()
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

void App::workersInit()
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

   const std::lock_guard<Mutex> lock(mutexWorkersRunning);
   this->workersRunning = true;
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

   const std::lock_guard<Mutex> lock(mutexWorkersRunning);
   this->workersRunning = false;
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

   // storage tragets
   log->log(Log_WARNING, std::string("Storage targets: ") +
      StringTk::uintToStr(storageTargets->getTargets().size()));

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

void App::daemonize()
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
   const unsigned waitTimeoutMS = 0; // infinite wait
   const unsigned nameResolutionRetries = 3;

   unsigned udpListenPort = cfg->getConnStoragePortUDP();
   unsigned udpMgmtdPort = cfg->getConnMgmtdPortUDP();
   std::string mgmtdHost = cfg->getSysMgmtdHost();
   NicAddressList nicList = getLocalNicList();

   RegistrationDatagramListener regDGramLis(netFilter, nicList, ackStore, udpListenPort,
      this->cfg->getConnRestrictOutboundInterfaces() );

   regDGramLis.start();

   log->log(Log_CRITICAL, "Waiting for beegfs-mgmtd@" +
      mgmtdHost + ":" + StringTk::uintToStr(udpMgmtdPort) + "...");


   bool gotMgmtd = NodesTk::waitForMgmtHeartbeat(
      this, &regDGramLis, mgmtNodes, mgmtdHost, udpMgmtdPort, waitTimeoutMS, nameResolutionRetries);

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

   NicAddressList nicList = getLocalNicList();
   RegisterNodeMsg msg(localNodeID, outLocalNodeNumID, NODETYPE_Storage, &nicList,
      cfg->getConnStoragePortUDP(), cfg->getConnStoragePortTCP() );

   Time startTime;
   Time lastRetryTime;
   unsigned nextRetryDelayMS = 0;

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

      const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
            NETMSGTYPE_RegisterNodeResp);

      if (respMsg)
      { // communication successful
         RegisterNodeRespMsg* respMsgCast = (RegisterNodeRespMsg*)respMsg.get();

         outLocalNodeNumID = respMsgCast->getNodeNumID();

         if(!outLocalNodeNumID)
         { // mgmt rejected our registration
            log->logErr("ID reservation request was rejected by this mgmt node: " +
               mgmtNode->getTypedNodeID() );
         }
         else
            log->log(Log_WARNING, "Node ID reservation successful.");

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

   return bool(outLocalNodeNumID);
}

/**
 * Pre-register all currently unmapped targets to get a numeric ID from mgmt.
 *
 * @return true if pre-registration successful
 * @throw InvalidConfigException on target access error
 */
boost::optional<std::map<uint16_t, std::unique_ptr<StorageTarget>>> App::preregisterTargets(
      const NumNodeID localNodeNumID)
{
   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
   {
      log->logErr("Unexpected: No management node found in store during target pre-registration.");
      return boost::none;
   }

   // validate IDs for mapped targets (i.e. targets that already have a numID)

   std::map<uint16_t, std::unique_ptr<StorageTarget>> targets;

   for (const auto& path : cfg->getStorageDirectories())
   {
      std::string targetID;
      uint16_t targetNumID;
      uint16_t newTargetNumID;

      // read or create target string ID file
      StorageTk::readOrCreateTargetIDFile(path.str(), localNodeNumID, &targetID);

      // read target numeric ID file
      StorageTk::readNumTargetIDFile(path.str(), STORAGETK_TARGETNUMID_FILENAME, &targetNumID);

      // sanity check: make sure we don't have numID without stringID
      if(targetNumID && targetID.empty() )
      {
         log->logErr("Target sanity problem: "
            "Found targetNumID, but no corresponding string ID: " + path.str());

         return boost::none;
      }


      bool registerRes = preregisterTarget(*mgmtNode, targetID, targetNumID, &newTargetNumID);
      if(!registerRes)
      { // registration rejected
         return boost::none;
      }

      if(!targetNumID)
      { // got a new numID for this target
         log->log(Log_DEBUG, "Retrieved new numeric target ID: " +
            targetID + " -> " + StringTk::uintToStr(newTargetNumID) );

         StorageTk::createNumIDFile(path.str(), STORAGETK_TARGETNUMID_FILENAME, newTargetNumID);
      }

      try
      {
         targets[newTargetNumID] = boost::make_unique<StorageTarget>(path, newTargetNumID,
               *timerQueue, *mgmtNodes, *mirrorBuddyGroupMapper);
         targets[newTargetNumID]->setCleanShutdown(StorageTk::checkSessionFileExists(path.str()));
      }
      catch (const std::system_error& e)
      {
         LOG(GENERAL, ERR, "Error while initializing target directory.",
               ("component", e.what()),
               ("error", e.code().message()));
         return boost::none;
      }
   }

   return targets;
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

      const auto respMsg = MessagingTk::requestResponse(mgmtNode, msg,
            NETMSGTYPE_RegisterTargetResp);

      if (respMsg)
      { // communication successful
         RegisterTargetRespMsg* respMsgCast = (RegisterTargetRespMsg*)respMsg.get();

         *outNewTargetNumID = respMsgCast->getTargetNumID();

         if(!*outNewTargetNumID)
         { // mgmt rejected target registration
            log->logErr("Target ID reservation request was rejected by this mgmt node: " +
               mgmtNode.getTypedNodeID() );
         }
         else
            log->log(Log_DEBUG, "Target ID reservation successful.");

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
   NicAddressList nicList = getLocalNicList();

   // start temporary registration datagram listener

   RegistrationDatagramListener regDGramLis(netFilter, nicList, ackStore, udpListenPort,
      this->cfg->getConnRestrictOutboundInterfaces() );

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
         !InternodeSyncer::downloadAndSyncStoragePools() ||
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

      for (ZipIterRange<UInt16List, UInt8List, UInt8List>
               it(targetIDs, reachabilityStates, consistencyStates);
            !it.empty(); ++it)
      {
         const auto change = localChangedStates.find(*it()->first);
         if (change != localChangedStates.end())
         {
            *it()->second = change->second.reachabilityState;
            *it()->third = change->second.consistencyState;
         }
      }


      targetStateStore->syncStatesFromLists(targetIDs, reachabilityStates, consistencyStates);

      // If a local primary target needs a resync, wait for poffline timeout before reporting the
      // target to mgmt. this ensures that the target will never be use by clients that haven't yet
      // seen the state update to needs-resync.
      const std::chrono::milliseconds timeout(OfflineWaitTimeoutTk<Config>::calculate(cfg));
      for (const auto& mapping : storageTargets->getTargets())
      {
         auto& target = *mapping.second;

         if (target.getConsistencyState() == TargetConsistencyState_NEEDS_RESYNC &&
               mirrorBuddyGroupMapper->getBuddyState(target.getID()) == BuddyState_PRIMARY)
            target.setOfflineTimeout(timeout);
      }

      if (!InternodeSyncer::downloadAllExceededQuotaLists(storageTargets->getTargets()))
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

   for (const auto& mapping : storageTargets->getTargets())
   {
      auto& target = *mapping.second;

      const auto path = target.getPath().str() + "/" + STORAGETK_SESSIONS_BACKUP_FILE_NAME;

      bool pathRes = StorageTk::pathExists(path);
      if(!pathRes)
         continue;

      bool loadRes = this->sessions->loadFromFile(path, mapping.first);
      if(!loadRes)
      {
         this->log->logErr("Could not restore all sessions from file " + path + " ; targetID: " +
            StringTk::uintToStr(mapping.first) );
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

   for (const auto& mapping : storageTargets->getTargets())
   {
      auto& target = *mapping.second;

      const auto path = target.getPath().str() + "/" + STORAGETK_SESSIONS_BACKUP_FILE_NAME;

      bool pathRes = StorageTk::pathExists(path);
      if(pathRes)
         this->log->log(Log_WARNING, "Overwriting existing session file: " + path);

      bool saveRes = this->sessions->saveToFile(path, mapping.first);
      if(!saveRes)
      {
         this->log->logErr("Could not store all sessions to file " + path + "; "
            "targetID: " + StringTk::uintToStr(mapping.first) );
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

   for (const auto& mapping : storageTargets->getTargets())
   {
      auto& target = *mapping.second;

      const auto path = target.getPath().str() + "/" + STORAGETK_SESSIONS_BACKUP_FILE_NAME;

      bool pathRes = StorageTk::pathExists(path);
      if(!pathRes)
         continue;

      if(remove(path.c_str() ) )
      {
         this->log->logErr("Could not remove session file " + path + " ; targetID: "
            + StringTk::uintToStr(mapping.first) + "; SysErr: " + System::getErrString() );
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
         LOG(GENERAL, ERR, "Error loading " + std::string(APP_LIB_ZFS_NAME) + ". "
              "Please make sure the libzfs2 development packages are installed.",
              ("System error", dlerror()));
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
         LOG(GENERAL, ERR, "Error closing " + std::string(APP_LIB_ZFS_NAME) + ".",
               ("System error", dlerror()));
         libZfsErrorReported = true;
         return false;
      }
   }

   return true;
}

void App::checkTargetsUUIDs()
{
   if (!cfg->getStoreFsUUID().empty())
   {
      std::list<Path> paths = cfg->getStorageDirectories();
      std::list<std::string> uuid_strs = cfg->getStoreFsUUID();

      if (paths.size() != uuid_strs.size()) {
         throw InvalidConfigException("Storage path list and storage UUID list have different sizes");
      }

      auto path = paths.begin();
      auto cfg_uuid_str = uuid_strs.begin();

      for(; path != paths.end() && cfg_uuid_str != uuid_strs.end(); ++path, ++cfg_uuid_str) {

         // Find out device numbers of underlying device
         struct stat st;

         if (stat(path->str().c_str(), &st)) {
            throw InvalidConfigException("Could not stat target directory: " + path->str());
         }

         // look for the device path
         std::ifstream mountInfo("/proc/self/mountinfo");

         if (!mountInfo) {
            throw InvalidConfigException("Could not open /proc/self/mountinfo");
         }

         auto majmin_f = boost::format("%1%:%2%") % (st.st_dev >> 8) % (st.st_dev & 0xFF);         

         std::string line, device_path, device_majmin;
         while (std::getline(mountInfo, line)) {
            std::istringstream is(line);
            std::string dummy;
            is >> dummy >> dummy >> device_majmin >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> device_path;

            if (majmin_f.str() == device_majmin)
               break;

            device_path = "";
         }

         if (device_path.empty()) {
            throw InvalidConfigException("Determined the underlying device for target directory " +  path->str() + " is " + majmin_f.str() + " but could not find that device in /proc/self/mountinfo");
         }

         // Lookup the fs UUID
         std::unique_ptr<blkid_struct_probe, void(*)(blkid_struct_probe*)>
               probe(blkid_new_probe_from_filename(device_path.data()), blkid_free_probe);

         if (!probe) {
            throw InvalidConfigException("Failed to open device for probing: " + device_path + " (check BeeGFS is running with root or sufficient privileges)");
         }

         if (blkid_probe_enable_superblocks(probe.get(), 1) < 0) {
            throw InvalidConfigException("Failed to enable superblock probing");
         }

         if (blkid_do_fullprobe(probe.get()) < 0) {
            throw InvalidConfigException("Failed to probe device");
         }

         const char* uuid = nullptr; // gets released automatically
         if (blkid_probe_lookup_value(probe.get(), "UUID", &uuid, nullptr) < 0) {
            throw InvalidConfigException("Failed to lookup file system UUID");
         }

         std::string uuid_str(uuid);

         if (*cfg_uuid_str != uuid_str)
         {
            throw InvalidConfigException("UUID of the file system under the storage target "
                  + path->str() + " (" + uuid_str
                  + ") does not match the one configured (" + *cfg_uuid_str + ")");
         }

      }
   }
   else
   {
      LOG(GENERAL, WARNING, "UUIDs of targets underlying file systems have not been configured and will "
            "therefore not be checked. To prevent starting the server accidentally with the wrong "
            "data, it is strongly recommended to set the storeFsUUID config parameter to "
            "the appropriate UUIDs.");
   }

}

/**
 * Creates a list of Enterprise Features that are in use. This list is passed to logEULAMsg.
 */
bool App::checkEnterpriseFeatureUsage()
{
   std::string enabledFeatures;

   if (this->mirrorBuddyGroupMapper->getSize() > 0)
      enabledFeatures.append("Mirroring, ");

   if (this->cfg->getQuotaEnableEnforcement())
      enabledFeatures.append("Quotas, ");

   if (this->storagePoolStore->getSize() > 1)
      enabledFeatures.append("Storage Pools, ");

   // Remove ", " from end of string
   if (enabledFeatures.length() > 2)
      enabledFeatures.resize(enabledFeatures.size() - 2);

   logEULAMsg(enabledFeatures);

   // return true only if any enterprise features are enabled, false otherwise
   return !enabledFeatures.empty();
}
