#include <common/app/log/LogContext.h>
#include <common/components/worker/queue/UserWorkContainer.h>
#include <common/components/worker/DummyWork.h>
#include <common/components/ComponentInitException.h>
#include <common/components/RegistrationDatagramListener.h>
#include <common/net/message/nodes/RegisterNodeMsg.h>
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include <common/net/sock/RDMASocket.h>
#include <common/nodes/LocalNode.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/NodesTk.h>
#include <components/FileEventLogger.h>
#include <components/ModificationEventFlusher.h>
#include <components/DisposalGarbageCollector.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include <storage/MetadataEx.h>
#include <toolkit/BuddyCommTk.h>
#include <toolkit/StorageTkEx.h>
#include <boost/format.hpp>
#include "App.h"

#include <array>
#include <mntent.h>
#include <csignal>
#include <fstream>
#include <sstream>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>
#include <blkid/blkid.h>
#include <uuid/uuid.h>

// this magic number is not available on all supported platforms. specifically, rhel5 does not have
// linux/magic.h (which is where this constant is found).
#ifndef EXT3_SUPER_MAGIC
 #define EXT3_SUPER_MAGIC  0xEF53
#endif


#define APP_WORKERS_DIRECT_NUM      1
#define APP_SYSLOG_IDENTIFIER       "beegfs-meta"


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
   this->clientNodes = NULL;
   this->metaCapacityPools = NULL;
   this->targetMapper = NULL;
   this->storageBuddyGroupMapper = NULL;
   this->metaBuddyGroupMapper = NULL;
   this->targetStateStore = NULL;
   this->metaStateStore = NULL;
   this->metaBuddyCapacityPools = NULL;
   this->workQueue = NULL;
   this->commSlaveQueue = NULL;
   this->disposalDir = NULL;
   this->buddyMirrorDisposalDir = NULL;
   this->rootDir = NULL;
   this->metaStore = NULL;
   this->ackStore = NULL;
   this->sessions = NULL;
   this->mirroredSessions = NULL;
   this->nodeOperationStats = NULL;
   this->netMessageFactory = NULL;
   this->inodesPath = NULL;
   this->dentriesPath = NULL;
   this->buddyMirrorInodesPath = NULL;
   this->buddyMirrorDentriesPath = NULL;
   this->dgramListener = NULL;
   this->connAcceptor = NULL;
   this->statsCollector = NULL;
   this->internodeSyncer = NULL;
   this->modificationEventFlusher = NULL;
   this->timerQueue = new TimerQueue(1, 1);
   this->gcQueue = new TimerQueue(1, 1);
   this->buddyResyncer = NULL;

   this->nextNumaBindTarget = 0;
}

App::~App()
{
   // Note: Logging of the common lib classes is not working here, because this is called
   // from class Program (so the thread-specific app-pointer isn't set in this context).

   commSlavesDelete();
   workersDelete();

   SAFE_DELETE(this->buddyResyncer);
   SAFE_DELETE(this->timerQueue);
   SAFE_DELETE(this->modificationEventFlusher);
   SAFE_DELETE(this->internodeSyncer);
   SAFE_DELETE(this->statsCollector);
   SAFE_DELETE(this->connAcceptor);

   streamListenersDelete();

   SAFE_DELETE(this->dgramListener);
   SAFE_DELETE(this->dentriesPath);
   SAFE_DELETE(this->inodesPath);
   SAFE_DELETE(this->buddyMirrorDentriesPath);
   SAFE_DELETE(this->buddyMirrorInodesPath);
   SAFE_DELETE(this->netMessageFactory);
   SAFE_DELETE(this->nodeOperationStats);
   SAFE_DELETE(this->sessions);
   SAFE_DELETE(this->mirroredSessions);
   SAFE_DELETE(this->ackStore);
   if(this->disposalDir && this->metaStore)
      this->metaStore->releaseDir(this->disposalDir->getID() );
   if(this->buddyMirrorDisposalDir && this->metaStore)
      this->metaStore->releaseDir(this->buddyMirrorDisposalDir->getID() );
   if(this->rootDir && this->metaStore)
      this->metaStore->releaseDir(this->rootDir->getID() );
   SAFE_DELETE(this->metaStore);
   SAFE_DELETE(this->commSlaveQueue);
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
   SAFE_DELETE(this->metaStateStore);
   SAFE_DELETE(this->targetStateStore);
   SAFE_DELETE(this->metaCapacityPools);
   SAFE_DELETE(this->log);
   SAFE_DELETE(this->tcpOnlyFilter);
   SAFE_DELETE(this->netFilter);

   SAFE_DELETE(this->cfg);

   delete timerQueue;

   Logger::destroyLogger();
   closelog();
}

/**
 * Initialize config and run app either in normal mode or in special unit tests mode.
 */
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
      std::cerr << "[BeeGFS Metadata Node Version: " << BEEGFS_VERSION << std::endl;
      std::cerr << "Refer to the default config file (/etc/beegfs/beegfs-meta.conf)" << std::endl;
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


   // init basic data objects & storage
   NumNodeID localNodeNumID;
   std::string localNodeID;

   // locks working dir => call before anything else that accesses the disk
   const bool targetNew = preinitStorage();

   initLogging();
   checkTargetUUID();
   initLocalNodeIDs(localNodeID, localNodeNumID);
   initDataObjects();
   initBasicNetwork();

   initStorage();
   initXAttrLimit();
   initRootDir(localNodeNumID);
   initDisposalDir();

   registerSignalHandler();

   // ACLs need enabled client side XAttrs in order to work.
   if (cfg->getStoreClientACLs() && !cfg->getStoreClientXAttrs() )
      throw InvalidConfigException(
            "Client ACLs are enabled in config file, but extended attributes are not. "
            "ACLs cannot be stored without extended attributes.");


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

   // Find MgmtNode
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

      bool preregisterRes = preregisterNode(localNodeID, localNodeNumID);
      if(!preregisterRes)
         throw InvalidConfigException("Pre-registration at management node canceled");
   }

   if (!localNodeNumID) // just a sanity check that should never fail
      throw InvalidConfigException("Failed to retrieve numeric local node ID from mgmt");

   // we have all local node data now => init localNode

   initLocalNode(localNodeID, localNodeNumID);
   initLocalNodeNumIDFile(localNodeNumID);

   // Keeps the local node state from the static call to the InternodeSyncer method so we can pass
   // it when we construct the actual object.
   TargetConsistencyState initialConsistencyState;

   bool downloadRes = downloadMgmtInfo(initialConsistencyState);
   if (!downloadRes)
   {
      log->log(1, "Downloading target states from management node failed. Shutting down...");
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }

   // Check for the sessions file. If there is none, it's either the first run, or we crashed so we
   // need a resync.
   bool sessionFilePresent = StorageTk::checkSessionFileExists(metaPathStr);
   if (!targetNew && !sessionFilePresent)
      initialConsistencyState = TargetConsistencyState_NEEDS_RESYNC;

   // init components

   BuddyCommTk::prepareBuddyNeedsResyncState(*mgmtNodes->referenceFirstNode(),
         *metaBuddyGroupMapper, *timerQueue, localNode->getNumID());

   try
   {
      initComponents(initialConsistencyState);
   }
   catch(ComponentInitException& e)
   {
      log->logErr(e.what() );
      log->log(Log_CRITICAL, "A hard error occurred. Shutting down...");
      appResult = APPCODE_INITIALIZATION_ERROR;
      return;
   }

   // restore sessions from last clean shut down
   restoreSessions();

   // check and log if enterprise features are used
   checkEnterpriseFeatureUsage();

   // log system and configuration info

   logInfos();

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
   InternodeSyncer::syncClients({}, false);

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

   if (!cfg->getFileEventLogTarget().empty())
      fileEventLogger.reset(new FileEventLogger(cfg->getFileEventLogTarget()));
}

/**
 * Init basic shared objects like work queues, node stores etc.
 */
void App::initDataObjects()
{
   this->mgmtNodes = new NodeStoreServers(NODETYPE_Mgmt, true);
   this->metaNodes = new NodeStoreServers(NODETYPE_Meta, true);
   this->storageNodes = new NodeStoreServers(NODETYPE_Storage, false);
   this->clientNodes = new NodeStoreClients();

   NicAddressList nicList;

   this->targetMapper = new TargetMapper();
   this->storageNodes->attachTargetMapper(targetMapper);

   this->storageBuddyGroupMapper = new MirrorBuddyGroupMapper(targetMapper);
   this->metaBuddyGroupMapper = new MirrorBuddyGroupMapper();

   this->metaCapacityPools = new NodeCapacityPools(
         false, DynamicPoolLimits(0, 0, 0, 0, 0, 0), DynamicPoolLimits(0, 0, 0, 0, 0, 0) );
   this->metaNodes->attachCapacityPools(metaCapacityPools);

   this->metaBuddyCapacityPools = new NodeCapacityPools(
      false, DynamicPoolLimits(0, 0, 0, 0, 0, 0), DynamicPoolLimits(0, 0, 0, 0, 0, 0) );
   this->metaBuddyGroupMapper->attachMetaCapacityPools(metaBuddyCapacityPools);

   this->targetStateStore = new TargetStateStore(NODETYPE_Storage);
   this->targetMapper->attachStateStore(targetStateStore);

   this->metaStateStore = new TargetStateStore(NODETYPE_Meta);
   this->metaNodes->attachStateStore(metaStateStore);

   this->storagePoolStore = boost::make_unique<StoragePoolStore>(storageBuddyGroupMapper,
                                                                 targetMapper);
   // add newly mapped targets and buddy groups to storage pool store
   this->targetMapper->attachStoragePoolStore(storagePoolStore.get());
   this->storageBuddyGroupMapper->attachStoragePoolStore(storagePoolStore.get());

   this->targetMapper->attachExceededQuotaStores(&exceededQuotaStores);

   this->workQueue = new MultiWorkQueue();
   this->commSlaveQueue = new MultiWorkQueue();

   if(cfg->getTuneUsePerUserMsgQueues() )
      workQueue->setIndirectWorkList(new UserWorkContainer() );

   this->ackStore = new AcknowledgmentStore();

   this->sessions = new SessionStore();
   this->mirroredSessions = new SessionStore();

   this->nodeOperationStats = new MetaNodeOpStats();

   this->isRootBuddyMirrored = false;
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
   std::string interfacesList = cfg->getConnInterfacesList();
   if(!interfacesList.empty() )
   {
      log->log(Log_DEBUG, "Allowed interfaces: " + interfacesList);
      StringTk::explodeEx(interfacesList, ',', true, &allowedInterfaces);
   }

   findAllowedInterfaces(localNicList);

   if(localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

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
void App::initLocalNodeIDs(std::string& outLocalID, NumNodeID& outLocalNumID)
{
   // load (or generate) nodeID and compare to original nodeID

   Path metaPath(metaPathStr);
   Path nodeIDPath = metaPath / STORAGETK_NODEID_FILENAME;
   StringList nodeIDList; // actually contains only a single line

   bool idPathExists = StorageTk::pathExists(nodeIDPath.str());
   if(idPathExists)
      ICommonConfig::loadStringListFile(nodeIDPath.str().c_str(), nodeIDList);

   if(!nodeIDList.empty() )
      outLocalID = *nodeIDList.begin();

   // auto-generate nodeID if it wasn't loaded

   if(outLocalID.empty() )
      outLocalID = System::getHostname();

   // check for nodeID changes

   StorageTk::checkOrCreateOrigNodeIDFile(metaPathStr, outLocalID);


   // load nodeNumID file
   StorageTk::readNumIDFile(metaPath.str(), STORAGETK_NODENUMID_FILENAME, &outLocalNumID);

   // note: localNodeNumID is still 0 here if it wasn't loaded from the file
}

/**
 * create and attach the localNode object, store numID in storage dir
 */
void App::initLocalNode(std::string& localNodeID, NumNodeID localNodeNumID)
{
   unsigned portUDP = cfg->getConnMetaPortUDP();
   unsigned portTCP = cfg->getConnMetaPortTCP();
   NicAddressList nicList = getLocalNicList();

   // create localNode object
   localNode = std::make_shared<LocalNode>(NODETYPE_Meta, localNodeID, localNodeNumID, portUDP,
         portTCP, nicList);

   // attach to metaNodes store
   metaNodes->setLocalNode(this->localNode);
}

/**
 * Store numID file in storage directory
 */
void App::initLocalNodeNumIDFile(NumNodeID localNodeNumID)
{
   StorageTk::createNumIDFile(metaPathStr, STORAGETK_NODENUMID_FILENAME, localNodeNumID.val());
}

/**
 * this contains things that would actually live inside initStorage() but need to be
 * done at an earlier stage (like working dir locking before log file creation).
 *
 * note: keep in mind that we don't have the logger here yet, because logging can only be
 * initialized after the working dir has been locked within this method.
 *
 * @returns true if there was no storageFormatFile before (target was uninitialized)
 */
bool App::preinitStorage()
{
   Path metaPath(cfg->getStoreMetaDirectory() );
   this->metaPathStr = metaPath.str(); // normalize

   if(metaPath.empty() )
      throw InvalidConfigException("No metadata storage directory specified");

   if(!metaPath.absolute() ) /* (check to avoid problems after chdir later) */
      throw InvalidConfigException("Path to storage directory must be absolute: " + metaPathStr);

   const bool formatFileExists = StorageTk::checkStorageFormatFileExists(metaPathStr);

   if(!cfg->getStoreAllowFirstRunInit() &&
      !formatFileExists)
      throw InvalidConfigException("Storage directory not initialized and "
         "initialization has been disabled: " + metaPathStr);

   this->pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile() ); // ignored if pidFile not defined

   if(!StorageTk::createPathOnDisk(metaPath, false) )
      throw InvalidConfigException("Unable to create metadata directory: " + metaPathStr +
         " (" + System::getErrString(errno) + ")" );

   this->workingDirLockFD = StorageTk::lockWorkingDirectory(cfg->getStoreMetaDirectory() );
   if (!workingDirLockFD.valid())
      throw InvalidConfigException("Unable to lock working directory: " + metaPathStr);

   return !formatFileExists;
}

void App::initStorage()
{
   // change working dir to meta directory
   int changeDirRes = chdir(metaPathStr.c_str() );
   if(changeDirRes)
   { // unable to change working directory
      throw InvalidConfigException("Unable to change working directory to: " + metaPathStr + " "
         "(SysErr: " + System::getErrString() + ")");
   }

   // storage format file
   if(!StorageTkEx::createStorageFormatFile(metaPathStr) )
      throw InvalidConfigException("Unable to create storage format file in: " +
         cfg->getStoreMetaDirectory() );

   StorageTkEx::checkStorageFormatFile(metaPathStr);

   // dentries directory
   dentriesPath = new Path(META_DENTRIES_SUBDIR_NAME);
   StorageTk::initHashPaths(*dentriesPath,
      META_DENTRIES_LEVEL1_SUBDIR_NUM, META_DENTRIES_LEVEL2_SUBDIR_NUM);

   // buddy mirrored dentries directory
   buddyMirrorDentriesPath = new Path(META_BUDDYMIRROR_SUBDIR_NAME "/" META_DENTRIES_SUBDIR_NAME);
   StorageTk::initHashPaths(*buddyMirrorDentriesPath,
      META_DENTRIES_LEVEL1_SUBDIR_NUM, META_DENTRIES_LEVEL2_SUBDIR_NUM);

   // inodes directory
   inodesPath = new Path(META_INODES_SUBDIR_NAME);
   if(!StorageTk::createPathOnDisk(*this->inodesPath, false) )
      throw InvalidConfigException("Unable to create directory: " +  inodesPath->str() );
   StorageTk::initHashPaths(*inodesPath,
      META_INODES_LEVEL1_SUBDIR_NUM, META_INODES_LEVEL2_SUBDIR_NUM);

   // buddy mirrored inodes directory
   buddyMirrorInodesPath = new Path(META_BUDDYMIRROR_SUBDIR_NAME "/" META_INODES_SUBDIR_NAME);
   if(!StorageTk::createPathOnDisk(*this->buddyMirrorInodesPath, false) )
      throw InvalidConfigException(
         "Unable to create directory: " + buddyMirrorInodesPath->str());
   StorageTk::initHashPaths(*buddyMirrorInodesPath,
      META_INODES_LEVEL1_SUBDIR_NUM, META_INODES_LEVEL2_SUBDIR_NUM);

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

void App::initXAttrLimit()
{
   // check whether the filesystem supports overly many amounts of xattrs (>64kb list size).
   // of the filesystems we support, this is currently only xfs.
   // also check for filesystems mounted beneath the metadata root dir, if any are found, limit the
   // xattrs too (it's probably not worth it to check the fs types here, since the setup should be
   // rare.)
   if (!cfg->getStoreUseExtendedAttribs())
      return;

   cfg->setLimitXAttrListLength(true);

   struct statfs metaRootStat;

   if (::statfs(cfg->getStoreMetaDirectory().c_str(), &metaRootStat))
   {
      LOG(GENERAL, CRITICAL, "Could not statfs() meta root directory.", sysErr);
      throw InvalidConfigException("Could not statfs() meta root directory.");
   }

   // ext3 and ext4 have the same magic, and are currently the only "safe" filesystems officially
   // supported.
   if (metaRootStat.f_type == EXT3_SUPER_MAGIC)
      cfg->setLimitXAttrListLength(false);
   else
   {
      LOG(GENERAL, NOTICE, "Limiting number of xattrs per inode.");
      return;
   }

   // the metadata root directory does not support overly long xattrs. check for filesystems mounted
   // beneath the metadata root, and enable xattrs limiting if any are found.
   std::string metaRootPath(PATH_MAX, '\0');

   if (!realpath(cfg->getStoreMetaDirectory().c_str(), &metaRootPath[0]))
   {
      LOG(GENERAL, CRITICAL, "Could not check meta root dir for xattr compatibility.", sysErr);
      throw InvalidConfigException("Could not check meta root dir for xattr compatibility.");
   }

   metaRootPath.resize(strlen(metaRootPath.c_str()));
   metaRootPath += '/';

   FILE* mounts = setmntent("/etc/mtab", "r");
   if (!mounts)
   {
      LOG(GENERAL, CRITICAL, "Could not open mtab.", sysErr);
      throw InvalidConfigException("Could not open mtab.");
   }

   struct mntent mountBuf;
   char buf[PATH_MAX * 4];

   struct mntent* mount;

   errno = 0;
   while ((mount = getmntent_r(mounts, &mountBuf, buf, sizeof(buf))))
   {
      if (strstr(mount->mnt_dir, metaRootPath.c_str()) == mount->mnt_dir)
      {
         cfg->setLimitXAttrListLength(true);
         break;
      }
   }

   endmntent(mounts);

   if (errno)
   {
      LOG(GENERAL, ERR, "Could not read mtab.", sysErr);
      throw InvalidConfigException("Could not read mtab.");
   }

   if (cfg->getLimitXAttrListLength())
      LOG(GENERAL, NOTICE, "Limiting number of xattrs per inode.");
}

void App::initRootDir(NumNodeID localNodeNumID)
{
   // try to load root dir from disk (through metaStore) or create a new one

   this->metaStore = new MetaStore();

   // try to reference root directory with buddy mirroring
   rootDir = this->metaStore->referenceDir(META_ROOTDIR_ID_STR, true, true);

   // if that didn't work try to reference non-buddy-mirrored root dir
   if (!rootDir)
   {
      rootDir = this->metaStore->referenceDir(META_ROOTDIR_ID_STR, false, true);
   }

   if(rootDir)
   { // loading succeeded (either with or without mirroring => init rootNodeID
      this->log->log(Log_NOTICE, "Root directory loaded.");

      NumNodeID rootDirOwner = rootDir->getOwnerNodeID();
      bool rootIsBuddyMirrored = rootDir->getIsBuddyMirrored();

      // try to set rootDirOwner as root node
      if (rootDirOwner && metaRoot.setIfDefault(rootDirOwner, rootIsBuddyMirrored))
      { // new root node accepted (check if rootNode is localNode)
         NumNodeID primaryRootDirOwner;
         if (rootIsBuddyMirrored)
            primaryRootDirOwner = NumNodeID(
               metaBuddyGroupMapper->getPrimaryTargetID(rootDirOwner.val() ) );
         else
            primaryRootDirOwner = rootDirOwner;

         if(localNodeNumID == primaryRootDirOwner)
         {
            log->log(Log_CRITICAL, "I got root (by possession of root directory)");
            if (rootIsBuddyMirrored)
               log->log(Log_CRITICAL, "Root directory is mirrored");
         }
         else
            log->log(Log_CRITICAL,
               "Root metadata server (by possession of root directory): " + rootDirOwner.str());
      }
   }
   else
   { // failed to load root directory => create a new rootDir (not mirrored)
      this->log->log(Log_CRITICAL,
         "This appears to be a new storage directory. Creating a new root dir.");

      UInt16Vector stripeTargets;
      unsigned defaultChunkSize = this->cfg->getTuneDefaultChunkSize();
      unsigned defaultNumStripeTargets = this->cfg->getTuneDefaultNumStripeTargets();
      Raid0Pattern stripePattern(defaultChunkSize, stripeTargets, defaultNumStripeTargets);

      DirInode newRootDir(META_ROOTDIR_ID_STR,
         S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO, 0, 0, NumNodeID(), stripePattern, false);

      this->metaStore->makeDirInode(newRootDir);
      this->rootDir = this->metaStore->referenceDir(META_ROOTDIR_ID_STR, false, true);

      if(!this->rootDir)
      { // error
         this->log->logErr("Failed to store root directory. Unable to proceed.");
         throw InvalidConfigException("Failed to store root directory");
      }
   }

}

void App::initDisposalDir()
{
   // try to load disposal dir from disk (through metaStore) or create a new one

   this->disposalDir = this->metaStore->referenceDir(META_DISPOSALDIR_ID_STR, false, true);
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

      DirInode newDisposalDir(META_DISPOSALDIR_ID_STR,
         S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO, 0, 0, NumNodeID(), stripePattern, false);

      this->metaStore->makeDirInode(newDisposalDir);
      this->disposalDir = this->metaStore->referenceDir(META_DISPOSALDIR_ID_STR, false, true);

      if(!this->disposalDir)
      { // error
         this->log->logErr("Failed to store disposal directory. Unable to proceed.");
         throw InvalidConfigException("Failed to store disposal directory");
      }

   }

   buddyMirrorDisposalDir = metaStore->referenceDir(META_MIRRORDISPOSALDIR_ID_STR, true, true);
   if(buddyMirrorDisposalDir)
   { // loading succeeded
      log->log(Log_DEBUG, "Mirrored disposal directory loaded.");
   }
   else
   { // failed to load disposal directory => create a new one
      log->log(Log_DEBUG, "Creating a new mirrored disposal directory.");

      UInt16Vector stripeTargets;
      unsigned defaultChunkSize = cfg->getTuneDefaultChunkSize();
      unsigned defaultNumStripeTargets = cfg->getTuneDefaultNumStripeTargets();
      Raid0Pattern stripePattern(defaultChunkSize, stripeTargets, defaultNumStripeTargets);

      DirInode newDisposalDir(META_MIRRORDISPOSALDIR_ID_STR,
         S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO, 0, 0, NumNodeID(), stripePattern, true);

      metaStore->makeDirInode(newDisposalDir);
      buddyMirrorDisposalDir = metaStore->referenceDir(META_MIRRORDISPOSALDIR_ID_STR, true, true);

      if(!buddyMirrorDisposalDir)
      { // error
         log->logErr("Failed to store mirrored disposal directory. Unable to proceed.");
         throw InvalidConfigException("Failed to store mirrored disposal directory");
      }

   }

}

void App::initComponents(TargetConsistencyState initialConsistencyState)
{
   this->log->log(Log_DEBUG, "Initializing components...");

   NicAddressList nicList = getLocalNicList();
   this->dgramListener = new DatagramListener(
      netFilter, nicList, ackStore, cfg->getConnMetaPortUDP(),
      this->cfg->getConnRestrictOutboundInterfaces() );
   if(cfg->getTuneListenerPrioShift() )
      dgramListener->setPriorityShift(cfg->getTuneListenerPrioShift() );

   streamListenersInit();

   unsigned short listenPort = cfg->getConnMetaPortTCP();

   this->connAcceptor = new ConnAcceptor(this, nicList, listenPort);

   this->statsCollector = new StatsCollector(workQueue, STATSCOLLECTOR_COLLECT_INTERVAL_MS,
      STATSCOLLECTOR_HISTORY_LENGTH);

   this->buddyResyncer = new BuddyResyncer();

   this->internodeSyncer = new InternodeSyncer(initialConsistencyState);

   this->modificationEventFlusher = new ModificationEventFlusher();

   workersInit();
   commSlavesInit();

   this->log->log(Log_DEBUG, "Components initialized.");
}

void App::startComponents()
{
   log->log(Log_DEBUG, "Starting up components...");

   // make sure child threads don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();

   timerQueue->start();
   gcQueue->start();

   this->dgramListener->start();

   // wait for nodes list download before we start handling client requests

   PThread::unblockInterruptSignals(); // temporarily unblock interrupt, so user can cancel waiting

   PThread::blockInterruptSignals(); // reblock signals for next child threads

   streamListenersStart();

   this->connAcceptor->start();

   this->statsCollector->start();

   this->internodeSyncer->start();

   timerQueue->enqueue(std::chrono::minutes(5),
         [] { InternodeSyncer::downloadAndSyncClients(true); });

   this->modificationEventFlusher->start();

   if(const auto wait = getConfig()->getTuneDisposalGCPeriod()) {
       this->gcQueue->enqueue(std::chrono::seconds(wait), disposalGarbageCollector);
   }

   workersStart();
   commSlavesStart();

   PThread::unblockInterruptSignals(); // main app thread may receive SIGINT/SIGTERM

   log->log(Log_DEBUG, "Components running.");
}

void App::stopComponents()
{

   SAFE_DELETE(this->gcQueue);

   // note: this method may not wait for termination of the components, because that could
   //    lead to a deadlock (when calling from signal handler)

   // note: no commslave stop here, because that would keep workers from terminating

   if(modificationEventFlusher) // The modificationEventFlusher has to be stopped before the
                                // workers, because it tries to notify all the workers about the
                                // changed modification state.
      modificationEventFlusher->selfTerminate();

   // resyncer wants to control the workers, so any running resync must be finished or aborted
   // before the workers are stopped.
   if(buddyResyncer)
      buddyResyncer->shutdown();

   workersStop();

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

void App::updateLocalNicList(NicAddressList& localNicList)
{
   std::vector<AbstractNodeStore*> allNodes({ mgmtNodes, metaNodes, storageNodes, clientNodes});
   updateLocalNicListAndRoutes(log, localNicList, allNodes);
   localNode->updateInterfaces(0, 0, localNicList);
   dgramListener->setLocalNicList(localNicList);
   connAcceptor->updateLocalNicList(localNicList);
}

void App::joinComponents()
{
   log->log(Log_DEBUG, "Joining component threads...");

   /* (note: we need one thread for which we do an untimed join, so this should be a quite reliably
      terminating thread) */
   statsCollector->join();

   workersJoin();

   waitForComponentTermination(modificationEventFlusher);
   waitForComponentTermination(dgramListener);
   waitForComponentTermination(connAcceptor);

   streamListenersJoin();

   waitForComponentTermination(internodeSyncer);

   commSlavesStop(); // placed here because otherwise it would keep workers from terminating
   commSlavesJoin();
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
      Worker* worker = new Worker(
         std::string("Worker") + StringTk::uintToStr(i+1), workQueue, QueueWorkType_INDIRECT);

      worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

      workerList.push_back(worker);
   }

   for(unsigned i=0; i < APP_WORKERS_DIRECT_NUM; i++)
   {
      Worker* worker = new Worker(
         std::string("DirectWorker") + StringTk::uintToStr(i+1), workQueue, QueueWorkType_DIRECT);

      worker->setBufLens(cfg->getTuneWorkerBufSize(), cfg->getTuneWorkerBufSize() );

      workerList.push_back(worker);
   }
}

void App::commSlavesInit()
{
   unsigned numCommSlaves = cfg->getTuneNumCommSlaves();

   for(unsigned i=0; i < numCommSlaves; i++)
   {
      Worker* worker = new Worker(
         std::string("CommSlave") + StringTk::uintToStr(i+1), commSlaveQueue, QueueWorkType_DIRECT);

      worker->setBufLens(cfg->getTuneCommSlaveBufSize(), cfg->getTuneCommSlaveBufSize() );

      commSlaveList.push_back(worker);
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
}

void App::commSlavesStart()
{
   unsigned numNumaNodes = System::getNumNumaNodes();

   for(WorkerListIter iter = commSlaveList.begin(); iter != commSlaveList.end(); iter++)
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
   for(WorkerListIter iter = workerList.begin(); iter != workerList.end(); iter++)
   {
      (*iter)->selfTerminate();

      // add dummy work to wake up the worker immediately for faster self termination
      PersonalWorkQueue* personalQ = (*iter)->getPersonalWorkQueue();
      workQueue->addPersonalWork(new DummyWork(), personalQ);
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

void App::commSlavesDelete()
{
   for(WorkerListIter iter = commSlaveList.begin(); iter != commSlaveList.end(); iter++)
   {
      delete(*iter);
   }

   commSlaveList.clear();
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

/**
 * Request mgmt heartbeat and wait for the mgmt node to appear in nodestore.
 *
 * @return true if mgmt heartbeat received, false on error or thread selftermination order
 */
bool App::waitForMgmtNode()
{
   const unsigned waitTimeoutMS = 0; // infinite wait
   const unsigned nameResolutionRetries = 3;

   unsigned udpListenPort = cfg->getConnMetaPortUDP();
   unsigned udpMgmtdPort = cfg->getConnMgmtdPortUDP();
   std::string mgmtdHost = cfg->getSysMgmtdHost();
   NicAddressList nicList = getLocalNicList();

   RegistrationDatagramListener regDGramLis(netFilter, nicList, ackStore, udpListenPort,
      this->cfg->getConnRestrictOutboundInterfaces());
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

   NumNodeID rootNodeID = metaRoot.getID();
   NicAddressList nicList = getLocalNicList();

   RegisterNodeMsg msg(localNodeID, outLocalNodeNumID, NODETYPE_Meta, &nicList,
      cfg->getConnMetaPortUDP(), cfg->getConnMetaPortTCP() );
   msg.setRootNumID(rootNodeID);

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
 * Downloads the list of nodes, targets and buddy groups (for meta and storage servers) from the
 * mgmtd.
 *
 * @param outInitialConsistencyState The consistency state the local meta node has on the mgmtd
 *                                   before any state reports are sent.
 */
bool App::downloadMgmtInfo(TargetConsistencyState& outInitialConsistencyState)
{
   Config* cfg = this->getConfig();

   int retrySleepTimeMS = 10000; // 10sec
   unsigned udpListenPort = cfg->getConnMetaPortUDP();
   bool allSuccessful = false;
   NicAddressList nicList = getLocalNicList();

   // start temporary registration datagram listener
   RegistrationDatagramListener regDGramLis(netFilter, nicList, ackStore, udpListenPort,
      this->cfg->getConnRestrictOutboundInterfaces() );
   regDGramLis.start();

   // loop until we're registered and everything is downloaded (or until we got interrupted)
   do
   {
      // register ourselves
      // (note: node registration needs to be done before downloads to get notified of updates)
      if (!InternodeSyncer::registerNode(&regDGramLis) )
         continue;

      // download all mgmt info the HBM cares for
      if (!InternodeSyncer::downloadAndSyncNodes() ||
          !InternodeSyncer::downloadAndSyncTargetMappings() ||
          !InternodeSyncer::downloadAndSyncStoragePools() ||
          !InternodeSyncer::downloadAndSyncTargetStatesAndBuddyGroups() ||
          !InternodeSyncer::updateMetaCapacityPools() ||
          !InternodeSyncer::updateMetaBuddyCapacityPools())
         continue;

      InternodeSyncer::downloadAndSyncClients(false);

      // ...and then the InternodeSyncer's part.
      if (!InternodeSyncer::updateMetaStatesAndBuddyGroups(outInitialConsistencyState, false) )
         continue;

      if(!InternodeSyncer::downloadAllExceededQuotaLists(storagePoolStore->getPoolsAsVec()))
         continue;

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

   std::string path = this->metaPathStr  + "/" + std::string(STORAGETK_SESSIONS_BACKUP_FILE_NAME);
   std::string mpath = this->metaPathStr  + "/" + std::string(STORAGETK_MSESSIONS_BACKUP_FILE_NAME);

   bool pathRes = StorageTk::pathExists(path);
   bool mpathRes = StorageTk::pathExists(mpath);
   if (!pathRes && !mpathRes)
      return false;

   if (pathRes)
   {
      bool loadRes = this->sessions->loadFromFile(path, *metaStore);
      if (!loadRes)
      {
         log->logErr("Could not restore all sessions");
         retVal = false;
      }
   }

   if (mpathRes)
   {
      bool loadRes = this->mirroredSessions->loadFromFile(mpath, *metaStore);
      if (!loadRes)
      {
         log->logErr("Could not restore all mirrored sessions");
         retVal = false;
      }
   }

   log->log(Log_NOTICE, "Restored "
         + StringTk::uintToStr(sessions->getSize()) + " sessions and "
         + StringTk::uintToStr(mirroredSessions->getSize()) + " mirrored sessions");

   return retVal;
}

bool App::storeSessions()
{
   bool retVal = true;

   std::string path = this->metaPathStr + "/" + std::string(STORAGETK_SESSIONS_BACKUP_FILE_NAME);
   std::string mpath = this->metaPathStr + "/" + std::string(STORAGETK_MSESSIONS_BACKUP_FILE_NAME);

   if (StorageTk::pathExists(path))
      log->log(Log_WARNING, "Overwriting existing session file");

   bool saveRes = this->sessions->saveToFile(path);
   if(!saveRes)
   {
      this->log->logErr("Could not store all sessions to file " + path);
      retVal = false;
   }

   if (StorageTk::pathExists(mpath))
      log->log(Log_WARNING, "Overwriting existing mirror session file");

   saveRes = this->mirroredSessions->saveToFile(mpath);
   if(!saveRes)
   {
      this->log->logErr("Could not store all mirror sessions to file " + mpath);
      retVal = false;
   }

   if (retVal)
      log->log(Log_NOTICE, "Stored "
            + StringTk::uintToStr(sessions->getSize()) + " sessions and "
            + StringTk::uintToStr(mirroredSessions->getSize()) + " mirrored sessions");

   return retVal;
}

bool App::deleteSessionFiles()
{
   bool retVal = true;

   std::string path = this->metaPathStr + "/" + std::string(STORAGETK_SESSIONS_BACKUP_FILE_NAME);
   std::string mpath = this->metaPathStr + "/" + std::string(STORAGETK_MSESSIONS_BACKUP_FILE_NAME);

   bool pathRes = StorageTk::pathExists(path);
   bool mpathRes = StorageTk::pathExists(mpath);
   if (!pathRes && !mpathRes)
      return retVal;

   if (pathRes && remove(path.c_str()))
   {
      log->logErr("Could not remove sessions file");
      retVal = false;
   }

   if (mpathRes && remove(mpath.c_str()))
   {
      log->logErr("Could not remove mirrored sessions file");
      retVal = false;
   }

   return retVal;
}

void App::checkTargetUUID() 
{
   if (!cfg->getStoreFsUUID().empty())
   {
      Path metaPath(cfg->getStoreMetaDirectory() );

      // Find out device numbers of underlying device
      struct stat st;

      if (stat(metaPath.str().c_str(), &st)) {
         throw InvalidConfigException("Could not stat metadata directory: " + metaPathStr);
      }

      // look for the device path
      std::ifstream mountInfo("/proc/self/mountinfo");

      if (!mountInfo) {
         throw InvalidConfigException("Could not open /proc/self/mountinfo");
      }

      auto majmin_f = boost::format("%1%:%2%") % major(st.st_dev) % minor(st.st_dev);

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
         throw InvalidConfigException("Determined the underlying device for metadata directory " + metaPathStr + " is " + majmin_f.str() + " but could not find that device in /proc/self/mountinfo");
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

      if (cfg->getStoreFsUUID() != uuid_str)
      {
         throw InvalidConfigException("UUID of the metadata file system (" + uuid_str
               + ") does not match the one configured (" + cfg->getStoreFsUUID() + ")");
      }
   }
   else
   {
      LOG(GENERAL, WARNING, "UUID of underlying file system has not been configured and will "
            "therefore not be checked. To prevent starting the server accidentally with the wrong "
            "data, it is strongly recommended to set the storeFsUUID config parameter to "
            "the appropriate UUID.");
   }

}

/**
 * Creates a list of Enterprise Features that are in use. This list is passed to logEULAMsg.
 */
bool App::checkEnterpriseFeatureUsage()
{
   std::string enabledFeatures;

   if (this->metaBuddyGroupMapper->getSize() > 0)
      enabledFeatures.append("Mirroring, ");

   if (this->cfg->getQuotaEnableEnforcement())
      enabledFeatures.append("Quotas, ");

   if (this->cfg->getStoreClientACLs())
      enabledFeatures.append("Access Control Lists, ");

   if (this->storagePoolStore->getSize() > 1)
      enabledFeatures.append("Storage Pools, ");

   // Remove ", " from end of string
   if (enabledFeatures.length() > 2)
      enabledFeatures.resize(enabledFeatures.size() - 2);

   logEULAMsg(enabledFeatures);

   // return true only if any enterprise features are enabled, false otherwise
   return !enabledFeatures.empty();

}
