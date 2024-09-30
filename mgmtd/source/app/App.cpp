#include <common/app/log/LogContext.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/worker/IncSyncedCounterWork.h>
#include <common/components/ComponentInitException.h>
#include <common/nodes/LocalNode.h>
#include <common/nodes/DynamicPoolLimits.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "App.h"

#include <csignal>
#include <syslog.h>

#include <boost/lexical_cast.hpp>

#define APP_WORKERS_DIRECT_NUM   1
#define APP_SYSLOG_IDENTIFIER    "beegfs-mgmtd"

App::App(int argc, char** argv) :
   shuttingDown(App_RUNNING)
{
   this->argc = argc;
   this->argv = argv;

   this->appResult = APPCODE_NO_ERROR;

   this->cfg = NULL;
   this->netFilter = NULL;
   this->tcpOnlyFilter = NULL;
   this->log = NULL;
   this->localNodeNumID = 0; // 0 means invalid/undefined
   this->metaCapacityPools = NULL;
   this->targetNumIDMapper = NULL;
   this->targetMapper = NULL;
   this->storageBuddyGroupMapper = NULL;
   this->metaBuddyGroupMapper = NULL;
   this->targetStateStore = NULL;
   this->metaStateStore = NULL;
   this->metaBuddyCapacityPools = NULL;
   this->mgmtNodes = NULL;
   this->metaNodes = NULL;
   this->storageNodes = NULL;
   this->clientNodes = NULL;

   this->workQueue = NULL;
   this->ackStore = NULL;
   this->netMessageFactory = NULL;
   this->mgmtdPath = NULL;

   this->dgramListener = NULL;
   this->heartbeatMgr = NULL;
   this->streamListener = NULL;
   this->statsCollector = NULL;
   this->internodeSyncer = NULL;
   this->quotaManager = NULL;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   workersDelete();

   SAFE_DELETE(this->internodeSyncer);
   SAFE_DELETE(this->statsCollector);
   SAFE_DELETE(this->streamListener);
   SAFE_DELETE(this->heartbeatMgr);
   SAFE_DELETE(this->dgramListener);
   SAFE_DELETE(this->mgmtdPath);
   SAFE_DELETE(this->netMessageFactory);
   SAFE_DELETE(this->ackStore);
   SAFE_DELETE(this->workQueue);
   SAFE_DELETE(this->clientNodes);
   SAFE_DELETE(this->storageNodes);
   SAFE_DELETE(this->metaNodes);
   SAFE_DELETE(this->mgmtNodes);
   this->localNode.reset();
   SAFE_DELETE(this->metaBuddyCapacityPools);
   SAFE_DELETE(this->storageBuddyGroupMapper);
   SAFE_DELETE(this->metaBuddyGroupMapper);
   SAFE_DELETE(this->targetMapper);
   SAFE_DELETE(this->targetNumIDMapper);
   SAFE_DELETE(this->metaStateStore);
   SAFE_DELETE(this->targetStateStore);
   SAFE_DELETE(this->metaCapacityPools);
   SAFE_DELETE(this->log);
   SAFE_DELETE(this->tcpOnlyFilter);
   SAFE_DELETE(this->netFilter);
   SAFE_DELETE(this->quotaManager);

   SAFE_DELETE(this->cfg);

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
      std::cerr << "[BeeGFS Management Node Version: " << BEEGFS_VERSION << std::endl;
      std::cerr << "Refer to the default config file (/etc/beegfs/beegfs-mgmtd.conf)" << std::endl;
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
   const int SHUTDOWN_CHECK_TIMEOUT_SECS = 5;

   // init data objects & storage

   initDataObjects(argc, argv);

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

   // check and log if enterprise features are used
   checkEnterpriseFeatureUsage();

   // log system and configuration info

   logInfos();

   // detach process
   if(cfg->getRunDaemonized() )
      daemonize();

   // start component threads

   startComponents();

   while (shuttingDown.read() == App_RUNNING)
      ::sleep(SHUTDOWN_CHECK_TIMEOUT_SECS);

   shutDown(shuttingDown.read() == App_SHUTDOWN_CLEAN);

   // wait for termination

   joinComponents();

   log->log(Log_CRITICAL, "All components stopped. Exiting now!");
}

void App::initDataObjects(int argc, char** argv)
{
   bool firstRun = !preinitStorage();

   this->netFilter = new NetFilter(cfg->getConnNetFilterFile() );
   this->tcpOnlyFilter = new NetFilter(cfg->getConnTcpOnlyFilterFile() );

   // (check absolute log path to avoid chdir() problems)
   Path logStdPath(cfg->getLogStdFile() );
   if(!logStdPath.empty() && !logStdPath.absolute())
      throw InvalidConfigException("Path to log file must be absolute");

   Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogNoDate(),
         cfg->getLogStdFile(), cfg->getLogNumLines(), cfg->getLogNumRotatedFiles());
   this->log = new LogContext("App");

   DynamicPoolLimits poolLimitsMetaSpace(cfg->getTuneMetaSpaceLowLimit(),
      cfg->getTuneMetaSpaceEmergencyLimit(),
      cfg->getTuneMetaSpaceNormalSpreadThreshold(),
      cfg->getTuneMetaSpaceLowSpreadThreshold(),
      cfg->getTuneMetaSpaceLowDynamicLimit(),
      cfg->getTuneMetaSpaceEmergencyDynamicLimit() );
   DynamicPoolLimits poolLimitsMetaInodes(cfg->getTuneMetaInodesLowLimit(),
      cfg->getTuneMetaInodesEmergencyLimit(),
      cfg->getTuneMetaInodesNormalSpreadThreshold(),
      cfg->getTuneMetaInodesLowSpreadThreshold(),
      cfg->getTuneMetaInodesLowDynamicLimit(),
      cfg->getTuneMetaInodesEmergencyDynamicLimit() );
   this->metaCapacityPools = new NodeCapacityPools(cfg->getTuneMetaDynamicPools(),
      poolLimitsMetaSpace, poolLimitsMetaInodes);
   this->metaBuddyCapacityPools = new NodeCapacityPools(
      false, DynamicPoolLimits(0, 0, 0, 0, 0, 0), DynamicPoolLimits(0, 0, 0, 0, 0, 0) );

   this->targetNumIDMapper = new NumericIDMapper();
   this->targetMapper = new TargetMapperEx();
   this->storageBuddyGroupMapper = new MirrorBuddyGroupMapperEx(
         CONFIG_STORAGEBUDDYGROUPMAPPINGS_FILENAME, this->targetMapper);

   this->storagePoolStore = boost::make_unique<StoragePoolStoreEx>(storageBuddyGroupMapper,
                                                                   targetMapper);

   // the storageBuddyGroupMapper holds a reference to the storagePoolStore and the
   // storagePoolStore holds a reference to the storageBuddyGroupMapper, because the information
   // contained depends on each other (in both directions)
   this->storageBuddyGroupMapper->attachStoragePoolStore(storagePoolStore.get());

   this->metaBuddyGroupMapper = new MirrorBuddyGroupMapperEx(
         CONFIG_METABUDDYGROUPMAPPINGS_FILENAME);

   this->mgmtNodes = new NodeStoreServersEx(NODETYPE_Mgmt);
   this->metaNodes = new NodeStoreServersEx(NODETYPE_Meta);
   this->storageNodes = new NodeStoreServersEx(NODETYPE_Storage);
   this->clientNodes = new NodeStoreClientsEx();

   this->metaNodes->attachCapacityPools(metaCapacityPools);

   this->metaBuddyGroupMapper->attachNodeStore(metaNodes);

   this->storageNodes->attachTargetMapper(targetMapper);

   this->targetStateStore = new MgmtdTargetStateStore(NODETYPE_Storage);
   this->targetMapper->attachStateStore(targetStateStore);

   // the targetMapper holds a reference to the storagePoolStore and the
   // storagePoolStore holds a reference to the targetMapper, because the information
   // contained depends on each other (in both directions)
   this->targetMapper->attachStoragePoolStore(storagePoolStore.get());

   this->metaStateStore = new MgmtdTargetStateStore(NODETYPE_Meta);
   this->metaNodes->attachStateStore(metaStateStore);

   this->workQueue = new MultiWorkQueue();
   this->ackStore = new AcknowledgmentStore();

   initLocalNodeInfo();
   this->mgmtNodes->setLocalNode(this->localNode);

   registerSignalHandler();

   initStorage(firstRun); // (required here for persistent objects)

   this->netMessageFactory = new NetMessageFactory();
}

void App::findAllowedInterfaces(NicAddressList& outList) const
{
   // discover local NICs and filter them
   NetworkInterfaceCard::findAllInterfaces(allowedInterfaces, outList);
   outList.sort(NetworkInterfaceCard::NicAddrComp{&allowedInterfaces});
}

void App::initLocalNodeInfo()
{
   unsigned portUDP = cfg->getConnMgmtdPortUDP();
   unsigned portTCP = cfg->getConnMgmtdPortTCP();

   // prepare filter for published local interfaces
   std::string interfacesList = cfg->getConnInterfacesList();
   if(!interfacesList.empty() )
   {
      log->log(Log_DEBUG, "Allowed interfaces: " + interfacesList);
      StringTk::explodeEx(interfacesList, ',', true, &allowedInterfaces);
   }

   findAllowedInterfaces(localNicList);
   if (localNicList.empty())
      throw InvalidConfigException("Couldn't find any usable NIC");

   noDefaultRouteNets = std::make_shared<NetVector>();
   if(!initNoDefaultRouteList(noDefaultRouteNets.get()))
      throw InvalidConfigException("Failed to parse connNoDefaultRoute");

   initRoutingTable();
   updateRoutingTable();

   // try to load localNodeID from file and fall back to auto-generated ID otherwise

   try
   {
      Path mgmtdPath(cfg->getStoreMgmtdDirectory() );
      Path nodeIDPath = mgmtdPath / STORAGETK_NODEID_FILENAME;
      StringList nodeIDList; // actually, the file would contain only a single line

      ICommonConfig::loadStringListFile(nodeIDPath.str().c_str(), nodeIDList);
      if(!nodeIDList.empty() )
         localNodeID = *nodeIDList.begin();
   }
   catch(InvalidConfigException& e) {}

   // auto-generate nodeID if it wasn't loaded

   if(localNodeID.empty() )
      localNodeID = System::getHostname();

   // hard-wired numID for mgmt, because we only have a single mgmt node

   localNodeNumID = 1;

   // create the local node
   NicAddressList nicList = getLocalNicList();

   localNode = std::make_shared<LocalNode>(NODETYPE_Mgmt, localNodeID, NumNodeID(localNodeNumID),
         portUDP, portTCP, nicList);
}

/**
 * @returns true if this is mgmtd directory has been initialized before (false on first run).
 */
bool App::preinitStorage()
{
   /* note: this contains things that would actually live inside initStorage() but need to be
      done at an earlier stage (like working dir locking before log file creation) */

   this->mgmtdPath = new Path(cfg->getStoreMgmtdDirectory() );
   std::string mgmtdPathStr = mgmtdPath->str();

   if(mgmtdPath->empty() )
      throw InvalidConfigException("No management storage directory specified");

   if(!mgmtdPath->absolute() ) /* (check to avoid problems after chdir) */
      throw InvalidConfigException("Path to storage directory must be absolute: " + mgmtdPathStr);

   const bool initialized = StorageTk::checkStorageFormatFileExists(mgmtdPathStr);

   if(!cfg->getStoreAllowFirstRunInit() && !initialized)
      throw InvalidConfigException("Storage directory not initialized and "
         "initialization has been disabled: " + mgmtdPathStr);

   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   // create storage directory

   if(!StorageTk::createPathOnDisk(*mgmtdPath, false) )
      throw InvalidConfigException("Unable to create mgmt storage directory: " +
         cfg->getStoreMgmtdDirectory() );

   // lock storage directory

   this->workingDirLockFD = StorageTk::lockWorkingDirectory(cfg->getStoreMgmtdDirectory() );
   if (!workingDirLockFD.valid())
      throw InvalidConfigException("Invalid working directory: locking failed");

   return initialized;
}

void App::initStorage(const bool firstRun)
{
   std::string mgmtdPathStr = mgmtdPath->str();
   StringMap formatProperties;
   int formatVersion;

   // change current working dir to storage directory

   int changeDirRes = chdir(mgmtdPathStr.c_str() );
   if(changeDirRes)
   { // unable to change working directory
      throw InvalidConfigException("Unable to change working directory to: " + mgmtdPathStr +
         "(SysErr: " + System::getErrString() + ")");
   }

   // storage format file

   if(!StorageTk::createStorageFormatFile(mgmtdPathStr, STORAGETK_FORMAT_LONG_NODE_IDS) )
      throw InvalidConfigException("Unable to create storage format file in: " +
         cfg->getStoreMgmtdDirectory() );

   StorageTk::loadStorageFormatFile(mgmtdPathStr, STORAGETK_FORMAT_MIN_VERSION,
         STORAGETK_FORMAT_7_1, formatVersion, formatProperties);

   // check for nodeID changes

   StorageTk::checkOrCreateOrigNodeIDFile(mgmtdPathStr, localNode->getID() );

   // load stored nodes

   metaNodes->setStorePath(CONFIG_METANODES_FILENAME);
   storageNodes->setStorePath(CONFIG_STORAGENODES_FILENAME);
   clientNodes->setStorePath(CONFIG_CLIENTNODES_FILENAME);

   if (metaNodes->loadFromFile(&metaRoot, formatVersion < STORAGETK_FORMAT_7_1))
      LOG(GENERAL, NOTICE, "Loaded meta nodes.", ("node count", metaNodes->getSize()));

   // Fill the meta nodes' target state store (Note: The storage target state store is filled by
   // the TargetMapper, since it contains target IDs, not node IDs).
   metaNodes->fillStateStore();

   if (storageNodes->loadFromFile(nullptr, formatVersion < STORAGETK_FORMAT_7_1))
      LOG(GENERAL, NOTICE, "Loaded storage nodes.", ("node count", storageNodes->getSize()));

   if (clientNodes->loadFromFile(formatVersion < STORAGETK_FORMAT_7_1))
      LOG(GENERAL, NOTICE, "Loaded client nodes.", ("node count", clientNodes->getSize()));

   if (formatVersion < STORAGETK_FORMAT_7_1) {
      if (!metaNodes->saveToFile(&metaRoot))
         throw InvalidConfigException("Failed to write upgraded meta node store");
      if (!storageNodes->saveToFile(nullptr))
         throw InvalidConfigException("Failed to write upgraded storage node store");
      if (!clientNodes->saveToFile())
         throw InvalidConfigException("Failed to write upgraded client node store");
   }

   // load mapped targetNumIDs

   Path targetNumIDsPath(CONFIG_TARGETNUMIDS_FILENAME);
   targetNumIDMapper->setStorePath(targetNumIDsPath.str() );
   bool targetNumIDsLoaded = targetNumIDMapper->loadFromFile();
   if(targetNumIDsLoaded)
      log->log(Log_DEBUG, "Loaded target numeric ID mappings: " +
         StringTk::intToStr(targetNumIDMapper->getSize() ) );

   // load storage target pools
   Path storagePoolsPath(CONFIG_STORAGEPOOLS_FILENAME);
   storagePoolStore->setStorePath(storagePoolsPath.str() );
   if(storagePoolStore->loadFromFile() )
      this->log->log(Log_NOTICE, "Loaded storage pools: "
         + StringTk::intToStr(storagePoolStore->getSize() ) );

   // loading of target mappings and buddy group MUST happen after loading of storage pools.
   // target mappings and buddy groups might then add all targets and groups, which might be
   // missing from the storage pools file. This is especially important in case of
   // an upgrade. Normally new targets are automatically added to the default
   // pool. However, if we upgrade from an old version, the targets are not
   // newly registered, but read from disk, in which case the auto-add will
   // not happen. So we have to use a "fallback" here.

   // Set path for legacy resync set file
   Path targetsToResyncPath(CONFIG_STORAGETARGETSTORESYNC_FILENAME);
   targetStateStore->setResyncSetStorePath(targetsToResyncPath.str() );
   Path nodesToResyncPath(CONFIG_METANODESTORESYNC_FILENAME);
   metaStateStore->setResyncSetStorePath(nodesToResyncPath.str() );

   // Set path for state set file
   targetStateStore->setTargetStateStorePath(CONFIG_STORAGETARGETSTATES_FILENAME);
   metaStateStore->setTargetStateStorePath(CONFIG_METANODESTATES_FILENAME);

   readTargetStates(firstRun, formatProperties, targetStateStore);
   readTargetStates(firstRun, formatProperties, metaStateStore);

   // load target mappings
   Path targetsPath(CONFIG_TARGETMAPPINGS_FILENAME);
   targetMapper->setStorePath(targetsPath.str() );
   if(targetMapper->loadFromFile() )
      this->log->log(Log_NOTICE, "Loaded target mappings: " +
         StringTk::intToStr(targetMapper->getSize() ) );

   // load storage target mirror buddy group mappings
   // check the v6 location first. if that fails to load, try to move the 2015 file to the new
   // location and try to load again.
   for (int i = 0; i < 2; i++)
   {
      if (storageBuddyGroupMapper->loadFromFile() )
      {
         this->log->log(Log_NOTICE, "Loaded storage target mirror buddy group mappings: " +
            StringTk::intToStr(storageBuddyGroupMapper->getSize() ) );
         break;
      }

      const int renameRes = ::rename(CONFIG_BUDDYGROUPMAPPINGS_FILENAME_OLD,
            CONFIG_STORAGEBUDDYGROUPMAPPINGS_FILENAME);
      if (renameRes != 0)
         break;
   }

   // load meta node mirror buddy group mappings
   if (metaBuddyGroupMapper->loadFromFile() )
      this->log->log(Log_NOTICE, "Loaded metadata node mirror buddy group mappings: " +
         StringTk::intToStr(metaBuddyGroupMapper->getSize() ) );

   // save config, just to be sure that the format is correct and up to date
   formatProperties.erase("longNodeIDs");
   if (!StorageTk::writeFormatFile(mgmtdPathStr, STORAGETK_FORMAT_7_1, &formatProperties))
      throw InvalidConfigException("Could not write format.conf");

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
 * Restore target and node states from file.
 * If the file does not exist, also try the legacy file.
 * The file paths need to be set in the state store before calling this function.
 */
void App::readTargetStates(const bool firstRun, StringMap& formatProperties,
      MgmtdTargetStateStore* stateStore)
{
   if (stateStore->loadStatesFromFile())
   {  // file could be read, so no need to proceed further
      return;
   }

   std::string propertiesKey;

   switch (stateStore->getNodeType())
   {
      case NODETYPE_Meta:
         propertiesKey = STORAGETK_FORMAT_STATES_META;
         break;
      case NODETYPE_Storage:
         propertiesKey = STORAGETK_FORMAT_STATES_STORAGE;
         break;
      default:
         throw ComponentInitException("Invalid node type: "
               + boost::lexical_cast<std::string>(stateStore->getNodeType()));
   }

   if (firstRun)
   {
      // The first run in this mgmtd directory - just set the flag.
      formatProperties[propertiesKey] = "1";
      stateStore->saveStatesToFile();
      return;
   }

   if (formatProperties[propertiesKey] == "1")
      throw ComponentInitException(
            "Could not load target or node states. "
            "The target and node states before the last shutdown are unknown. "
            "If you want to start beegfs-mgmtd please manually create the files "
            + mgmtdPath->str() + "/" CONFIG_STORAGETARGETSTATES_FILENAME " "
            "and " + mgmtdPath->str() + "/" CONFIG_METANODESTATES_FILENAME " "
            "with empty contents. "
            "The states will be recovered from the currently known states of the servers. "
            "NodeType: " + stateStore->nodeTypeStr(true) + ".");

   // The first run after an update - read the old file, and set the flag.
   if (targetStateStore->loadResyncSetFromFile())
   {
      LOG(STATES, NOTICE, "Restored resync set.", ("NodeType", stateStore->nodeTypeStr(true)));

      // Make an effort to unlink the resync set file
      ::unlink(stateStore->getResyncSetStorePath().c_str());
   }

   stateStore->saveStatesToFile();
   formatProperties[propertiesKey] = "1";
}


void App::initComponents()
{
   log->log(Log_DEBUG, "Initializing components...");

   NicAddressList nicList = getLocalNicList();
   this->dgramListener = new DatagramListener(
      netFilter, nicList, ackStore, cfg->getConnMgmtdPortUDP(),
      this->cfg->getConnRestrictOutboundInterfaces() );
   this->heartbeatMgr = new HeartbeatManager(dgramListener);

   // attach heartbeat manager to storagePoolStore to enable sending of modifications
   storagePoolStore->attachHeartbeatManager(heartbeatMgr);

   unsigned short listenPort = cfg->getConnMgmtdPortTCP();

   this->streamListener = new StreamListener(localNicList, workQueue, listenPort);

   this->statsCollector = new StatsCollector(workQueue, STATSCOLLECTOR_COLLECT_INTERVAL_MS,
      STATSCOLLECTOR_HISTORY_LENGTH);

   this->internodeSyncer = new InternodeSyncer();

   // init the quota related stuff if required
   if (cfg->getQuotaEnableEnforcement() )
      this->quotaManager = new QuotaManager();

   workersInit();

   log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   this->dgramListener->start();
   this->heartbeatMgr->start();

   this->streamListener->start();

   this->statsCollector->start();

   this->internodeSyncer->start();

   // start the quota related stuff if required
   if (cfg->getQuotaEnableEnforcement() )
      this->quotaManager->start();

   workersStart();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{
   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   workersStop();

   if(quotaManager)
      quotaManager->selfTerminate();

   if(statsCollector)
      statsCollector->selfTerminate();

   if(streamListener)
      streamListener->selfTerminate();

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

void App::updateLocalNicList(NicAddressList& localNicList)
{
   std::vector<AbstractNodeStore*> allNodes({ mgmtNodes, metaNodes, storageNodes, clientNodes});
   updateLocalNicListAndRoutes(log, localNicList, allNodes);
   localNode->updateInterfaces(0, 0, localNicList);
   dgramListener->setLocalNicList(localNicList);
   // Updating StreamListener localNicList is not required. That is only
   // required for apps that create RDMA and SDP listeners.
   // Note that StreamListener::updateLocalNicList() does not exist. See
   // ConnAcceptor::updateLocalNicNicList().
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

   shuttingDown.set(App_SHUTDOWN_IMMEDIATE);
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
   log->log(4, "Joining component threads...");

   /* (note: we need one thread for which we do an untimed join, so this should be a quite reliably
      terminating thread) */
   this->statsCollector->join();

   workersJoin();

   waitForComponentTermination(dgramListener);
   waitForComponentTermination(heartbeatMgr);
   waitForComponentTermination(streamListener);

   if(quotaManager)
      waitForComponentTermination(quotaManager);
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
      this->log->log(Log_WARNING, std::string("Net filters: ") +
         StringTk::uintToStr(netFilter->getNumFilterEntries() ) );
   }

   if(tcpOnlyFilter->getNumFilterEntries() )
   {
      this->log->log(Log_WARNING, std::string("TCP-only filters: ") +
         StringTk::uintToStr(tcpOnlyFilter->getNumFilterEntries() ) );
   }
}

void App::shutDown (bool clean)
{
   getInternodeSyncer()->selfTerminate();
   waitForComponentTermination(internodeSyncer);

   if (clean &&
         (storageBuddyGroupMapper->getSize() != 0
         || metaBuddyGroupMapper->getSize() != 0))
   {
      setPrimariesPOffline();

      notifyWorkers();

      getHeartbeatMgr()->notifyAsyncRefreshTargetStates();
      getHeartbeatMgr()->saveNodeStores();

      int toWaitSecs = cfg->getSysTargetOfflineTimeoutSecs();
      LOG(GENERAL, NOTICE,
          "Shutdown in progress: Buddy groups frozen - waiting for clients to acknowledge.",
          ("Timeout (sec)", toWaitSecs));

      // POffline-Timeout = 0.5*OfflineTimeout; therefore clientTiemout here is 2*POfflineTimeout
      const int sleepTimeoutSecs = 1;

      while (toWaitSecs > 0)
      {
         if (shuttingDown.read() == App_SHUTDOWN_IMMEDIATE)
            break;

         ::sleep(sleepTimeoutSecs);
         toWaitSecs -= sleepTimeoutSecs;
      }
   }

   stopComponents();
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
   signal(sig, SIG_DFL); // reset the handler to its default.

   App* app = Program::getApp();

   // note: this might deadlock if the signal was thrown while the logger mutex is locked by the
   //    application thread (depending on whether the default mutex style is recursive). but
   //    even recursive mutexes are not acceptable in this case.
   //    we need something like a timed lock for the log mutex. if it succeeds within a
   //    few seconds, we know that we didn't hold the mutex lock. otherwise we simply skip the
   //    log message. this will only work if the mutex is non-recusive (which is unknown for
   //    the default mutex style).
   //    but it is very unlikely that the application thread holds the log mutex, because it
   //    joins the component threads and so it doesn't do anything else but sleeping!

   std::string signalStr;

   switch(sig)
   {
      case SIGINT:
         signalStr = "SIGINT";
         break;
      case SIGTERM:
         signalStr = "SIGTERM";
         break;
      default:
         signalStr = "(unknown)";
         break;
   }

   switch (app->shuttingDown.read())
   {
      case App_RUNNING:
         LOG(GENERAL, CRITICAL, "Received a signal " + signalStr + ". Clean shutdown initiated. "
               "Send another one to shutdown immediately.");
         app->shuttingDown.set(App_SHUTDOWN_CLEAN);
         signal(sig, App::signalHandler); // re-arm the signal handler
         break;
      case App_SHUTDOWN_CLEAN:
         LOG(GENERAL, CRITICAL, "Received a second signal. Forcing immediate shutdown.",
               ("Signal", signalStr));
         app->shuttingDown.set(App_SHUTDOWN_IMMEDIATE);
         break;
      default:
         break;
   }
}

void App::notifyWorkers()
{
   SynchronizedCounter notified;
   for (auto workerIt = workerList.begin(); workerIt != workerList.end(); ++workerIt)
   {
      PersonalWorkQueue* personalQ = (*workerIt)->getPersonalWorkQueue();
      workQueue->addPersonalWork(new IncSyncedCounterWork(&notified), personalQ);
   }

   notified.waitForCount(workerList.size());
}

void App::setPrimariesPOffline()
{
   UInt16List storageGroupIDs;
   UInt16List storagePrimaries;
   UInt16List storageSecondaries;
   storageBuddyGroupMapper->getMappingAsLists(storageGroupIDs, storagePrimaries,
         storageSecondaries);

   for (auto it = storagePrimaries.begin(); it != storagePrimaries.end(); ++it)
      targetStateStore->setReachabilityState(*it, TargetReachabilityState_POFFLINE);

   UInt16List metaGroupIDs;
   UInt16List metaPrimaries;
   UInt16List metaSecondaries;
   metaBuddyGroupMapper->getMappingAsLists(metaGroupIDs, metaPrimaries, metaSecondaries);

   for (auto it = metaPrimaries.begin(); it != metaPrimaries.end(); ++it)
      metaStateStore->setReachabilityState(*it, TargetReachabilityState_POFFLINE);
}

/**
 * Creates a list of Enterprise Features that are in use. This list is passed to logEULAMsg.
 */
bool App::checkEnterpriseFeatureUsage()
{
   std::string enabledFeatures;

   if (this->storageBuddyGroupMapper->getSize() > 0 || this->metaBuddyGroupMapper->getSize() > 0)
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
