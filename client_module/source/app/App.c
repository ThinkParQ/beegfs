#include <app/config/Config.h>
#include <app/log/Logger.h>
#include <app/App.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/TargetMapper.h>
#include <common/nodes/TargetStateStore.h>
#include <common/system/System.h>
#include <common/toolkit/ackstore/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <common/toolkit/Time.h>
#include <components/AckManager.h>
#include <components/DatagramListener.h>
#include <components/Flusher.h>
#include <components/InternodeSyncer.h>
#include <filesystem/FhgfsOpsPages.h>
#include <filesystem/FhgfsOpsInode.h>
#include <net/filesystem/FhgfsOpsCommKit.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <nodes/NodeStoreEx.h>
#include <toolkit/InodeRefStore.h>
#include <toolkit/NoAllocBufferStore.h>
#include <toolkit/StatFsCache.h>

// REMOVEME
#include <common/toolkit/Random.h>

#include <linux/xattr.h>

#ifndef BEEGFS_VERSION
   #error BEEGFS_VERSION undefined
#endif

/**
 * @param mountConfig belongs to the app afterwards (and will automatically be destructed
 * by the app)
 */
void App_init(App* this, MountConfig* mountConfig)
{
   this->mountConfig = mountConfig;

   this->appResult = APPCODE_NO_ERROR;

   this->connRetriesEnabled = true;
   this->netBenchModeEnabled = false;

   this->cfg = NULL;
   this->netFilter = NULL;
   this->tcpOnlyFilter = NULL;
   this->logger = NULL;
   this->fsUUID = NULL;
   StrCpyList_init(&this->allowedInterfaces);
   StrCpyList_init(&this->allowedRDMAInterfaces);
   UInt16List_init(&this->preferredMetaNodes);
   UInt16List_init(&this->preferredStorageTargets);
   this->cacheBufStore = NULL;
   this->pathBufStore = NULL;
   this->msgBufStore = NULL;
   this->ackStore = NULL;
   this->inodeRefStore = NULL;
   this->statfsCache = NULL;
   this->localNode = NULL;
   this->mgmtNodes = NULL;
   this->metaNodes = NULL;
   this->storageNodes = NULL;
   this->targetMapper = NULL;
   this->storageBuddyGroupMapper = NULL;
   this->metaBuddyGroupMapper = NULL;
   this->targetStateStore = NULL;
   this->metaStateStore = NULL;

   this->dgramListener = NULL;
   this->internodeSyncer = NULL;
   this->ackManager = NULL;
   this->flusher = NULL;

   this->fileInodeOps = NULL;
   this->symlinkInodeOps = NULL;
   this->dirInodeOps = NULL;
   this->specialInodeOps = NULL;

   Mutex_init(&this->nicListMutex);
   Mutex_init(&this->fsUUIDMutex);

#ifdef BEEGFS_DEBUG
   Mutex_init(&this->debugCounterMutex);

   this->numRPCs = 0;
   this->numRemoteReads = 0;
   this->numRemoteWrites = 0;
#endif // BEEGFS_DEBUG

}

void App_uninit(App* this)
{
   SAFE_DESTRUCT(this->flusher, Flusher_destruct);
   SAFE_DESTRUCT(this->ackManager, AckManager_destruct);
   SAFE_DESTRUCT(this->internodeSyncer, InternodeSyncer_destruct);
   SAFE_DESTRUCT(this->dgramListener, DatagramListener_destruct);

   SAFE_DESTRUCT(this->storageNodes, NodeStoreEx_destruct);
   SAFE_DESTRUCT(this->metaNodes, NodeStoreEx_destruct);
   SAFE_DESTRUCT(this->mgmtNodes, NodeStoreEx_destruct);

   if(this->logger)
      Logger_setAllLogLevels(this->logger, LOG_NOTHING); // disable logging

   SAFE_DESTRUCT(this->localNode, Node_put);
   SAFE_DESTRUCT(this->targetMapper, TargetMapper_destruct);
   SAFE_DESTRUCT(this->storageBuddyGroupMapper, MirrorBuddyGroupMapper_destruct);
   SAFE_DESTRUCT(this->metaBuddyGroupMapper, MirrorBuddyGroupMapper_destruct);
   SAFE_DESTRUCT(this->metaStateStore, TargetStateStore_destruct);
   SAFE_DESTRUCT(this->targetStateStore, TargetStateStore_destruct);
   SAFE_DESTRUCT(this->statfsCache, StatFsCache_destruct);
   SAFE_DESTRUCT(this->inodeRefStore, InodeRefStore_destruct);
   SAFE_DESTRUCT(this->ackStore, AcknowledgmentStore_destruct);
   SAFE_DESTRUCT(this->msgBufStore, NoAllocBufferStore_destruct);
   SAFE_DESTRUCT(this->pathBufStore, NoAllocBufferStore_destruct);
   SAFE_DESTRUCT(this->cacheBufStore, NoAllocBufferStore_destruct);
   UInt16List_uninit(&this->preferredStorageTargets);
   UInt16List_uninit(&this->preferredMetaNodes);
   StrCpyList_uninit(&this->allowedInterfaces);
   StrCpyList_uninit(&this->allowedRDMAInterfaces);
   SAFE_DESTRUCT(this->logger, Logger_destruct);
   SAFE_DESTRUCT(this->tcpOnlyFilter, NetFilter_destruct);
   SAFE_DESTRUCT(this->netFilter, NetFilter_destruct);
   SAFE_DESTRUCT(this->cfg, Config_destruct);

   AtomicInt_uninit(&this->lockAckAtomicCounter);

   SAFE_DESTRUCT(this->mountConfig, MountConfig_destruct);

   ListTk_kfreeNicAddressListElems(&this->rdmaNicList);
   NicAddressList_uninit(&this->rdmaNicList);

   kfree(this->fileInodeOps);
   kfree(this->symlinkInodeOps);
   kfree(this->dirInodeOps);
   kfree(this->specialInodeOps);

   Mutex_uninit(&this->nicListMutex);

#ifdef BEEGFS_DEBUG
   Mutex_uninit(&this->debugCounterMutex);
#endif // BEEGFS_DEBUG
}

int App_run(App* this)
{

   printk_fhgfs(KERN_INFO, "Built "
#ifdef BEEGFS_NVFS
      "with"
#else
      "without"
#endif
      " NVFS RDMA support.\n");

// init data objects & storage

   if(!__App_initDataObjects(this, this->mountConfig) )
   {
      printk_fhgfs(KERN_WARNING,
         "Configuration error: Initialization of common objects failed. "
         "(Log file may provide additional information.)\n");
      this->appResult = APPCODE_INVALID_CONFIG;
      return this->appResult;
   }

   if(!__App_initInodeOperations(this) )
   {
      printk_fhgfs(KERN_WARNING, "Initialization of inode operations failed.");
      this->appResult = APPCODE_INITIALIZATION_ERROR;
      return this->appResult;
   }


   // init components

   if(!__App_initComponents(this) )
   {
      printk_fhgfs(KERN_WARNING, "Component initialization error. "
         "(Log file may provide additional information.)\n");
      this->appResult = APPCODE_INITIALIZATION_ERROR;
      return this->appResult;
   }

   __App_logInfos(this);


   // start components

   __App_startComponents(this);

   // Note: We wait some ms for the node downloads here because the kernel would like to
   //    check the properties of the root directory directly after mount.

   InternodeSyncer_waitForMgmtInit(this->internodeSyncer, 1000);

   if(!__App_mountServerCheck(this) )
   { // mount check failed => cancel mount
      printk_fhgfs(KERN_WARNING, "Mount sanity check failed. Canceling mount. "
         "(Log file may provide additional information. Check can be disabled with "
         "sysMountSanityCheckMS=0 in the config file.)\n");
      this->appResult = APPCODE_INITIALIZATION_ERROR;

      return this->appResult;
   }

   // mark: mount succeeded if we got here!

   return this->appResult;
}

void App_stop(App* this)
{
   const char* logContext = "App (stop)";

   // note: be careful, because we don't know what has been succesfully initialized!

   // note: this can also be called from App_run() to cancel a mount!

   if(this->cfg)
   {
      __App_stopComponents(this);

      if(!Config_getConnUnmountRetries(this->cfg) )
         this->connRetriesEnabled = false;

      // wait for termination
      __App_joinComponents(this);

      if(this->logger)
         Logger_log(this->logger, 1, logContext, "All components stopped.");
   }
}

bool __App_initDataObjects(App* this, MountConfig* mountConfig)
{
   const char* logContext = "App (init data objects)";

   char* interfacesFilename;
   char* rdmaInterfacesFilename;
   char* preferredMetaFile;
   char* preferredStorageFile;
   size_t numCacheBufs;
   size_t cacheBufSize;
   size_t numPathBufs;
   size_t pathBufsSize;
   size_t numMsgBufs;
   size_t msgBufsSize;
   char* interfacesList;

   AtomicInt_init(&this->lockAckAtomicCounter, 0);

   this->cfg = Config_construct(mountConfig);
   if(!this->cfg)
      return false;


   if(!StringTk_hasLength(Config_getSysMgmtdHost(this->cfg) ) )
   {
      printk_fhgfs(KERN_WARNING, "Management host undefined\n");
      return false;
   }

   { // load net filter (required before any connection can be made, incl. local conns)
      const char* netFilterFile = Config_getConnNetFilterFile(this->cfg);

      this->netFilter = NetFilter_construct(netFilterFile);
      if(!this->netFilter)
         return false;
   }

   { // load tcp-only filter (required before any connection can be made, incl. local conns)
      const char* tcpOnlyFilterFile = Config_getConnTcpOnlyFilterFile(this->cfg);

      this->tcpOnlyFilter = NetFilter_construct(tcpOnlyFilterFile);
      if(!this->tcpOnlyFilter)
         return false;
   }

   // logger (+ hostname as initial clientID, real ID will be set below)
   this->logger = Logger_construct(this, this->cfg);
   if(Config_getLogClientID(this->cfg) )
   {
      const char* hostnameCopy = System_getHostnameCopy();
      Logger_setClientID(this->logger, hostnameCopy);
      kfree(hostnameCopy);
   }


   // load allowed interface list
   interfacesList = Config_getConnInterfacesList(this->cfg);
   if (StringTk_hasLength(interfacesList) )
      StringTk_explode(interfacesList, ',', &this->allowedInterfaces);
   else
   {
      // load allowed interface file
      interfacesFilename = Config_getConnInterfacesFile(this->cfg);
      if(strlen(interfacesFilename) &&
         !Config_loadStringListFile(interfacesFilename, &this->allowedInterfaces) )
      {
         Logger_logErrFormatted(this->logger, logContext,
            "Unable to load configured interfaces file: %s", interfacesFilename);
         return false;
      }
   }

   // load allowed RDMA interface file
   rdmaInterfacesFilename = Config_getConnRDMAInterfacesFile(this->cfg);
   if(strlen(rdmaInterfacesFilename) &&
      !Config_loadStringListFile(rdmaInterfacesFilename, &this->allowedRDMAInterfaces) )
   {
      Logger_logErrFormatted(this->logger, logContext,
         "Unable to load configured RDMA interfaces file: %s", rdmaInterfacesFilename);
      return false;
   }

   // load preferred nodes files
   preferredMetaFile = Config_getTunePreferredMetaFile(this->cfg);
   if(strlen(preferredMetaFile) &&
      !Config_loadUInt16ListFile(this->cfg, preferredMetaFile, &this->preferredMetaNodes) )
   {
      Logger_logErrFormatted(this->logger, logContext,
         "Unable to load configured preferred meta nodes file: %s", preferredMetaFile);
      return false;
   }

   preferredStorageFile = Config_getTunePreferredStorageFile(this->cfg);
   if(strlen(preferredStorageFile) &&
      !Config_loadUInt16ListFile(this->cfg, preferredStorageFile, &this->preferredStorageTargets) )
   {
      Logger_logErrFormatted(this->logger, logContext,
         "Unable to load configured preferred storage nodes file: %s", preferredStorageFile);
      return false;
   }

   // init stores, queues etc.

   this->targetMapper = TargetMapper_construct();

   this->storageBuddyGroupMapper = MirrorBuddyGroupMapper_construct();

   this->metaBuddyGroupMapper = MirrorBuddyGroupMapper_construct();

   this->targetStateStore = TargetStateStore_construct();

   this->metaStateStore = TargetStateStore_construct();

   this->mgmtNodes = NodeStoreEx_construct(this, NODETYPE_Mgmt);

   this->metaNodes = NodeStoreEx_construct(this, NODETYPE_Meta);

   this->storageNodes = NodeStoreEx_construct(this, NODETYPE_Storage);

   if(!__App_initLocalNodeInfo(this) )
      return false;

   if(Config_getLogClientID(this->cfg) )
   { // set real clientID
      NodeString alias;
      Node_copyAlias(this->localNode, &alias);
      Logger_setClientID(this->logger, alias.buf);
   }

   // prealloc buffers

   switch(Config_getTuneFileCacheTypeNum(this->cfg) )
   {
      case FILECACHETYPE_Buffered:
         numCacheBufs = Config_getTuneFileCacheBufNum(this->cfg);
         break;

      default:
         numCacheBufs = 0;
         break;
   }

   cacheBufSize = Config_getTuneFileCacheBufSize(this->cfg);

   this->cacheBufStore = NoAllocBufferStore_construct(numCacheBufs, cacheBufSize);
   if(!this->cacheBufStore)
      return false;

   numPathBufs = Config_getTunePathBufNum(this->cfg);
   pathBufsSize = Config_getTunePathBufSize(this->cfg);

   this->pathBufStore = NoAllocBufferStore_construct(numPathBufs, pathBufsSize);
   if(!this->pathBufStore)
      return false;

   numMsgBufs = Config_getTuneMsgBufNum(this->cfg);
   msgBufsSize = Config_getTuneMsgBufSize(this->cfg);

   this->msgBufStore = NoAllocBufferStore_construct(numMsgBufs, msgBufsSize);

   this->ackStore = AcknowledgmentStore_construct();

   this->inodeRefStore = InodeRefStore_construct();

   this->statfsCache = StatFsCache_construct(this);

   return true;
}

/**
 * Initialized the inode_operations structs depending on what features have been enabled in
 * the config.
 */
bool __App_initInodeOperations(App* this)
{
   Config* cfg = App_getConfig(this);

   this->fileInodeOps = os_kzalloc(sizeof(struct inode_operations) );
   this->symlinkInodeOps = os_kzalloc(sizeof(struct inode_operations) );
   this->dirInodeOps = os_kzalloc(sizeof(struct inode_operations) );
   this->specialInodeOps = os_kzalloc(sizeof(struct inode_operations) );

   if (!this->fileInodeOps || !this->symlinkInodeOps ||
       !this->dirInodeOps || !this->specialInodeOps)
   {
      SAFE_KFREE(this->fileInodeOps);
      SAFE_KFREE(this->symlinkInodeOps);
      SAFE_KFREE(this->dirInodeOps);
      SAFE_KFREE(this->specialInodeOps);

      return false;
   }

   this->fileInodeOps->getattr     = FhgfsOps_getattr;
   this->fileInodeOps->permission  = FhgfsOps_permission;
   this->fileInodeOps->setattr     = FhgfsOps_setattr;

#ifdef KERNEL_HAS_GENERIC_READLINK
   this->symlinkInodeOps->readlink    = generic_readlink; // default is fine for us currently
#endif
#ifdef KERNEL_HAS_GET_LINK
   this->symlinkInodeOps->get_link    = FhgfsOps_get_link;
#else
   this->symlinkInodeOps->follow_link = FhgfsOps_follow_link;
   this->symlinkInodeOps->put_link    = FhgfsOps_put_link;
#endif
   this->symlinkInodeOps->getattr     = FhgfsOps_getattr;
   this->symlinkInodeOps->permission  = FhgfsOps_permission;
   this->symlinkInodeOps->setattr     = FhgfsOps_setattr;

#ifdef KERNEL_HAS_ATOMIC_OPEN
   #ifdef BEEGFS_ENABLE_ATOMIC_OPEN
   this->dirInodeOps->atomic_open = FhgfsOps_atomicOpen;
   #endif // BEEGFS_ENABLE_ATOMIC_OPEN
#endif
   this->dirInodeOps->lookup      = FhgfsOps_lookupIntent;
   this->dirInodeOps->create      = FhgfsOps_createIntent;
   this->dirInodeOps->link        = FhgfsOps_link;
   this->dirInodeOps->unlink      = FhgfsOps_unlink;
   this->dirInodeOps->mknod       = FhgfsOps_mknod;
   this->dirInodeOps->symlink     = FhgfsOps_symlink;
   this->dirInodeOps->mkdir       = FhgfsOps_mkdir;
   this->dirInodeOps->rmdir       = FhgfsOps_rmdir;
   this->dirInodeOps->rename      = FhgfsOps_rename;
   this->dirInodeOps->getattr     = FhgfsOps_getattr;
   this->dirInodeOps->permission  = FhgfsOps_permission;
   this->dirInodeOps->setattr     = FhgfsOps_setattr;

   this->specialInodeOps->setattr = FhgfsOps_setattr;

   if (Config_getSysXAttrsEnabled(cfg) )
   {
      this->fileInodeOps->listxattr   = FhgfsOps_listxattr;
      this->dirInodeOps->listxattr   = FhgfsOps_listxattr;

#ifdef KERNEL_HAS_GENERIC_GETXATTR
      this->fileInodeOps->getxattr    = generic_getxattr;
      this->fileInodeOps->removexattr = FhgfsOps_removexattr;
      this->fileInodeOps->setxattr    = generic_setxattr;

      this->dirInodeOps->getxattr    = generic_getxattr;
      this->dirInodeOps->removexattr = FhgfsOps_removexattr;
      this->dirInodeOps->setxattr    = generic_setxattr;
#endif

      if (Config_getSysACLsEnabled(cfg) )
      {
#if defined(KERNEL_HAS_SET_ACL) || defined(KERNEL_HAS_SET_ACL_DENTRY)
         this->fileInodeOps->set_acl = FhgfsOps_set_acl;
         this->dirInodeOps->set_acl  = FhgfsOps_set_acl;
        /* Note: symlinks don't have ACLs
         * The get_acl() operation was introduced as get_inode_acl() in the struct inode_operations in Linux 6.2
         */
#if defined(KERNEL_HAS_GET_ACL)
         this->fileInodeOps->get_acl = FhgfsOps_get_acl;
         this->dirInodeOps->get_acl  = FhgfsOps_get_acl;
#endif
#if defined(KERNEL_HAS_GET_INODE_ACL)
         this->fileInodeOps->get_inode_acl = FhgfsOps_get_inode_acl;
         this->dirInodeOps->get_inode_acl  = FhgfsOps_get_inode_acl;
#endif
#else
         Logger_logErr(this->logger, "Init inode operations",
            "ACLs activated in config, but the signature inode->i_op->set_acl()"
            "not supported on this kernel version.");
         return false;
#endif // KERNEL_HAS_SET_ACL or KERNEL_HAS_SET_ACL_DENTRY
      }
   }

   return true;
}

/**
 * Retrieve NICs for the local node.
 *
 * @param nicList an uninitialized NicAddressList. Caller is responsible for
 *        memory management.
 */
void App_cloneLocalNicList(App* this, NicAddressList* nicList)
{
   Node_cloneNicList(this->localNode, nicList);
}

void App_updateLocalInterfaces(App* this, NicAddressList* nicList)
{
   NicAddressList rdmaNicList;
   NicListCapabilities nicCaps;
   NodeStoreEx* stores[] = {
      this->mgmtNodes, this->metaNodes, this->storageNodes, NULL
   };
   int i;

   App_findAllowedRDMAInterfaces(this, nicList, &rdmaNicList);

   Mutex_lock(&this->nicListMutex); // L O C K

   ListTk_kfreeNicAddressListElems(&this->rdmaNicList);
   NicAddressList_uninit(&this->rdmaNicList);
   ListTk_cloneNicAddressList(&rdmaNicList, &this->rdmaNicList, true);

   Mutex_unlock(&this->nicListMutex); // U N L O C K

   NIC_supportedCapabilities(nicList, &nicCaps);
   Node_updateInterfaces(this->localNode, Node_getPortUDP(this->localNode),
      Node_getPortTCP(this->localNode), nicList);
   Node_updateLocalInterfaces(this->localNode, nicList, &nicCaps, NULL);

   for (i = 0; stores[i] != NULL; ++i)
   {
      NodeStoreEx* store = stores[i];
      Node* node;

      for (node = NodeStoreEx_referenceFirstNode(store); node != NULL;
           node = NodeStoreEx_referenceNextNodeAndReleaseOld(store, node))
      {
         int nodeType = Node_getNodeType(node);
         Node_updateLocalInterfaces(node, nicList, &nicCaps,
            nodeType == NODETYPE_Meta || nodeType == NODETYPE_Storage? &rdmaNicList : NULL);
      }

   }
   ListTk_kfreeNicAddressListElems(&rdmaNicList);
   NicAddressList_uninit(&rdmaNicList);
}

/**
 * Retrieve file system UUID for the mounted BeeGFS.
 *
 * @return a pointer to a copy of the file system UUID. Must be freed by the caller.
 */
char* App_cloneFsUUID(App* this)
{
   char* fsUUID;
   Mutex_lock(&this->fsUUIDMutex);
   fsUUID = StringTk_strDup(this->fsUUID);
   Mutex_unlock(&this->fsUUIDMutex);

   return fsUUID;
}

/**
 * Update file system UUID for the mounted BeeGFS.
 *
 * @param fsUUID will be cloned and stored in the app object so original pointer can be freed.
 */
void App_updateFsUUID(App* this, const char* fsUUID)
{
   Mutex_lock(&this->fsUUIDMutex);
   this->fsUUID = StringTk_strDup(fsUUID);
   Mutex_unlock(&this->fsUUIDMutex);
}

/**
 * Populate nicList with the allowed and available interfaces.
 *
 * @param nicList uninitialized list to populate. Caller is responsible for memory
 *        management
 * @return true if an allowed and available NIC was found. nicList is initialized and
 *         contains elements.
 */
bool App_findAllowedInterfaces(App* this, NicAddressList* nicList)
{
   NicAddressList tmpNicList;
   bool useRDMA = Config_getConnUseRDMA(this->cfg);
   bool result;

   NicAddressList_init(&tmpNicList);
   NIC_findAll(&this->allowedInterfaces, useRDMA, false, &tmpNicList);

   if(!NicAddressList_length(&tmpNicList) )
   {
      result = false;
      goto exit;
   }

   // created sorted tmpNicList clone
   ListTk_cloneSortNicAddressList(&tmpNicList, nicList, &this->allowedInterfaces);

   result = true;

exit:
   if (result)
      ListTk_kfreeNicAddressListElems(&tmpNicList);
   NicAddressList_uninit(&tmpNicList);
   return result;
}

void App_cloneLocalRDMANicList(App* this, NicAddressList* rdmaNicList)
{
   Mutex_lock(&this->nicListMutex); // L O C K
   ListTk_cloneNicAddressList(&this->rdmaNicList, rdmaNicList, true);
   Mutex_unlock(&this->nicListMutex); // U N L O C K
}

void App_findAllowedRDMAInterfaces(App* this, NicAddressList* nicList, NicAddressList* rdmaNicList)
{
   const char* logContext = "App (find RDMA interfaces)";
   bool useRDMA = Config_getConnUseRDMA(this->cfg);
   if (useRDMA && StrCpyList_length(&this->allowedRDMAInterfaces) > 0)
   {
      NicAddressList tmpList;

      NicAddressList_init(&tmpList);
      NIC_findAll(&this->allowedRDMAInterfaces, true, true, &tmpList);

      if(!NicAddressList_length(&tmpList) )
      {
         Logger_logErr(this->logger, logContext, "Couldn't find any filtered and usable outbound RDMA NIC, using any");
         NicAddressList_init(rdmaNicList);
      }
      else
      {
         // created sorted rdmaNicList clone
         ListTk_cloneSortNicAddressList(&tmpList, rdmaNicList, &this->allowedRDMAInterfaces);
      }
      ListTk_kfreeNicAddressListElems(&tmpList);
      NicAddressList_uninit(&tmpList);
   }
   else
   {
      NicAddressList_init(rdmaNicList);
   }
}

bool __App_initLocalNodeInfo(App* this)
{
   const char* logContext = "App (init local node info)";

   NicAddressList nicList;
   char* hostname;
   Time now;
   pid_t currentPID;
   char* generatedAlias;
   char* alias;

   if (!App_findAllowedInterfaces(this, &nicList))
   {
      Logger_logErr(this->logger, logContext, "Couldn't find any usable NIC");
      // required by App_uninit()
      NicAddressList_init(&this->rdmaNicList);
      return false;
   }

   App_findAllowedRDMAInterfaces(this, &nicList, &this->rdmaNicList);

   // prepare clientID and create localNode
   Time_setToNowReal(&now);
   hostname = System_getHostnameCopy();
   currentPID = current->pid;

   // Generate the client alias (formerly nodeID):
   generatedAlias = StringTk_kasprintf("c%llX-%llX-%s",
      (uint64_t)currentPID, (uint64_t)now.tv_sec, hostname);

   // Truncate the generatedAlias at 32 characters before using it as the nodeID since it will be
   // used as this client's alias by the management, and aliases are limited to 32 characters.
   alias = os_kmalloc(33);
   strncpy(alias, generatedAlias, 32);
   alias[32] = '\0';

   // note: numeric ID gets initialized with 0; will be set by management later in InternodeSyncer
   this->localNode = Node_construct(this, alias, (NumNodeID){0},
      Config_getConnClientPort(this->cfg), 0, &nicList, NULL);

   // clean up
   kfree(generatedAlias);
   kfree(alias);
   kfree(hostname);

   // delete nicList elems
   ListTk_kfreeNicAddressListElems(&nicList);
   NicAddressList_uninit(&nicList);

   return true;
}

bool __App_initComponents(App* this)
{
   const char* logContext = "App (init components)";

   Logger_log(this->logger, Log_DEBUG, logContext, "Initializing components...");

   this->dgramListener =
      DatagramListener_construct(this, this->localNode, Config_getConnClientPort(this->cfg) );
   if(!this->dgramListener)
   {
      Logger_logErr(this->logger, logContext,
         "Initialization of DatagramListener component failed");
      return false;
   }

   this->internodeSyncer = InternodeSyncer_construct(this); // (requires dgramlis)

   this->ackManager = AckManager_construct(this);

   this->flusher = Flusher_construct(this);

   Logger_log(this->logger, Log_DEBUG, logContext, "Components initialized.");

   return true;
}

void __App_startComponents(App* this)
{
   const char* logContext = "App (start components)";

   Logger_log(this->logger, Log_DEBUG, logContext, "Starting up components...");

   Thread_start( (Thread*)this->dgramListener);

   Thread_start( (Thread*)this->internodeSyncer);
   Thread_start( (Thread*)this->ackManager);

   if(Config_getTuneFileCacheTypeNum(this->cfg) == FILECACHETYPE_Buffered)
      Thread_start( (Thread*)this->flusher);

   Logger_log(this->logger, Log_DEBUG, logContext, "Components running.");
}

void __App_stopComponents(App* this)
{
   const char* logContext = "App (stop components)";

   if(this->logger)
      Logger_log(this->logger, Log_WARNING, logContext, "Stopping components...");

   if(this->flusher && Config_getTuneFileCacheTypeNum(this->cfg) == FILECACHETYPE_Buffered)
      Thread_selfTerminate( (Thread*)this->flusher);
   if(this->ackManager)
      Thread_selfTerminate( (Thread*)this->ackManager);
   if(this->internodeSyncer)
      Thread_selfTerminate( (Thread*)this->internodeSyncer);

   if(this->dgramListener)
      Thread_selfTerminate( (Thread*)this->dgramListener);
}

void __App_joinComponents(App* this)
{
   const char* logContext = "App (join components)";

   if(this->logger)
      Logger_log(this->logger, 4, logContext, "Waiting for components to self-terminate...");


   if(Config_getTuneFileCacheTypeNum(this->cfg) == FILECACHETYPE_Buffered)
      __App_waitForComponentTermination(this, (Thread*)this->flusher);

   __App_waitForComponentTermination(this, (Thread*)this->ackManager);
   __App_waitForComponentTermination(this, (Thread*)this->internodeSyncer);

   __App_waitForComponentTermination(this, (Thread*)this->dgramListener);
}

/**
 * @param component the thread that we're waiting for via join(); may be NULL (in which case this
 * method returns immediately)
 */
void __App_waitForComponentTermination(App* this, Thread* component)
{
   const char* logContext = "App (wait for component termination)";

   const int timeoutMS = 2000;

   bool isTerminated;

   if(!component)
      return;

   isTerminated = Thread_timedjoin(component, timeoutMS);
   if(!isTerminated)
   { // print message to show user which thread is blocking
      if(this->logger)
         Logger_logFormatted(this->logger, 2, logContext,
            "Still waiting for this component to stop: %s", Thread_getName(component) );

      Thread_join(component);

      if(this->logger)
         Logger_logFormatted(this->logger, 2, logContext,
            "Component stopped: %s", Thread_getName(component) );
   }
}

void __App_logInfos(App* this)
{
   const char* logContext = "App_logInfos";
   NodeString alias;
   size_t nicListStrLen = 1024;
   char* nicListStr = os_kmalloc(nicListStrLen);
   char* extendedNicListStr = os_kmalloc(nicListStrLen);
   NicAddressList nicList;
   NicAddressListIter nicIter;

   Node_cloneNicList(this->localNode, &nicList);

   // print software version (BEEGFS_VERSION)
   Logger_logFormatted(this->logger, 1, logContext, "BeeGFS Client Version: %s", BEEGFS_VERSION);

   // print debug version info
   LOG_DEBUG(this->logger, 1, logContext, "--DEBUG VERSION--");

   // print local clientID
   Node_copyAlias(this->localNode, &alias);
   Logger_logFormatted(this->logger, 2, logContext, "ClientID: %s", alias.buf);

   // list usable network interfaces
   NicAddressListIter_init(&nicIter, &nicList);
   nicListStr[0] = 0;
   extendedNicListStr[0] = 0;

   for( ; !NicAddressListIter_end(&nicIter); NicAddressListIter_next(&nicIter) )
   {
      const char* nicTypeStr;
      char* nicAddrStr;
      NicAddress* nicAddr = NicAddressListIter_value(&nicIter);
      char* tmpStr = os_kmalloc(nicListStrLen);

      if(nicAddr->nicType == NICADDRTYPE_RDMA)
         nicTypeStr = "RDMA";
      else
      if(nicAddr->nicType == NICADDRTYPE_STANDARD)
         nicTypeStr = "TCP";
      else
         nicTypeStr = "Unknown";

      // short NIC info
      snprintf(tmpStr, nicListStrLen, "%s%s(%s) ", nicListStr, nicAddr->name, nicTypeStr);
      strcpy(nicListStr, tmpStr); // tmpStr to avoid writing to a buffer that we're reading
         // from at the same time

      // extended NIC info
      nicAddrStr = NIC_nicAddrToString(nicAddr);
      snprintf(tmpStr, nicListStrLen, "%s\n+ %s", extendedNicListStr, nicAddrStr);
      strcpy(extendedNicListStr, tmpStr); // tmpStr to avoid writing to a buffer that we're
         // reading from at the same time
      kfree(nicAddrStr);

      SAFE_KFREE(tmpStr);
   }

   Logger_logFormatted(this->logger, 2, logContext, "Usable NICs: %s", nicListStr);
   Logger_logFormatted(this->logger, 4, logContext, "Extended list of usable NICs: %s",
      extendedNicListStr);

   // print net filters
   if(NetFilter_getNumFilterEntries(this->netFilter) )
   {
      Logger_logFormatted(this->logger, Log_WARNING, logContext, "Net filters: %d",
         (int)NetFilter_getNumFilterEntries(this->netFilter) );
   }

   if(NetFilter_getNumFilterEntries(this->tcpOnlyFilter) )
   {
      Logger_logFormatted(this->logger, Log_WARNING, logContext, "TCP-only filters: %d",
         (int)NetFilter_getNumFilterEntries(this->tcpOnlyFilter) );
   }

   // print preferred nodes and targets

   if(UInt16List_length(&this->preferredMetaNodes) )
   {
      Logger_logFormatted(this->logger, Log_WARNING, logContext, "Preferred metadata servers: %d",
         (int)UInt16List_length(&this->preferredMetaNodes) );
   }

   if(UInt16List_length(&this->preferredStorageTargets) )
   {
      Logger_logFormatted(this->logger, Log_WARNING, logContext, "Preferred storage targets: %d",
         (int)UInt16List_length(&this->preferredStorageTargets) );
   }

   // Print a warning message if user has enabled ACLs, but no XAttrs (in that case, XAttrs have
   // been auto-enabled which contradicts the config file.)
   if (Config_getSysXAttrsImplicitlyEnabled(this->cfg) )
   {
      Logger_log(this->logger, Log_WARNING, logContext,
         "WARNING: ACLs are enabled, but XAttrs are not. "
         "XAttrs are needed to store ACLs, so they have automatically been enabled. "
         "Please check configuration.");
   }

   // clean up
   SAFE_KFREE(nicListStr);
   SAFE_KFREE(extendedNicListStr);

   ListTk_kfreeNicAddressListElems(&nicList);
   NicAddressList_uninit(&nicList);

}

/**
 * Perform some basic sanity checks to deny mount in case of an error.
 */
bool __App_mountServerCheck(App* this)
{
   const char* logContext = "Mount sanity check";
   Config* cfg = this->cfg;

   unsigned waitMS = Config_getSysMountSanityCheckMS(cfg);

   bool retVal = false;
   bool retriesEnabledOrig = this->connRetriesEnabled;
   bool mgmtInitDone;
   FhgfsOpsErr statRootErr;
   fhgfs_stat statRootDetails;
   FhgfsOpsErr statStorageErr;
   int64_t storageSpaceTotal;
   int64_t storageSpaceFree;


   if(!waitMS)
      return true; // mount check disabled


   this->connRetriesEnabled = false; // NO _ R E T R I E S

   // wait for management init

   mgmtInitDone = InternodeSyncer_waitForMgmtInit(this->internodeSyncer, waitMS);
   if(!mgmtInitDone)
   {
      Logger_logErr(this->logger, logContext, "Waiting for management server initialization "
         "timed out. Are the server settings correct? Is the management daemon running?");

      goto error_resetRetries;
   }

   // check root metadata server

   statRootErr = FhgfsOpsRemoting_statRoot(this, &statRootDetails);
   if(statRootErr != FhgfsOpsErr_SUCCESS)
   {
      Logger_logErrFormatted(this->logger, logContext, "Retrieval of root directory entry "
         "failed. Are all metadata servers running and registered at the management daemon? "
         "(Error: %s)", FhgfsOpsErr_toErrString(statRootErr) );

      goto error_resetRetries;
   }

   // check storage servers

   statStorageErr = FhgfsOpsRemoting_statStoragePath(this, false,
      &storageSpaceTotal, &storageSpaceFree);

   if(statStorageErr != FhgfsOpsErr_SUCCESS)
   {
      Logger_logErrFormatted(this->logger, logContext, "Retrieval of storage server free space "
         "info failed. Are the storage servers running and registered at the management daemon? "
         "Did you remove a storage target directory on a server? (Error: %s)",
         FhgfsOpsErr_toErrString(statStorageErr) );

      goto error_resetRetries;
   }

   retVal = true;


error_resetRetries:
   this->connRetriesEnabled = retriesEnabledOrig; // R E S E T _ R E T R I E S

   return retVal;
}

/**
 * Note: This is actually a Program version, not an App version, but we have it here now because
 * app provides a stable closed source part and we want this version to be fixed at compile time.
 */
const char* App_getVersionStr(void)
{
   return BEEGFS_VERSION;
}
