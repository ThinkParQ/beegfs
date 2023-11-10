#ifndef APP_H_
#define APP_H_

#include <app/config/MountConfig.h>
#include <common/toolkit/list/PointerListIter.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/net/sock/NicAddressList.h>
#include <common/threading/AtomicInt.h>
#include <common/threading/Mutex.h>
#include <common/threading/Thread.h>
#include <common/Common.h>
#include <toolkit/BitStore.h>


// program return codes
#define APPCODE_NO_ERROR               0
#define APPCODE_PROGRAM_ERROR          1
#define APPCODE_INVALID_CONFIG         2
#define APPCODE_INITIALIZATION_ERROR   3
#define APPCODE_RUNTIME_ERROR          4

// forward declarations
struct Config;
struct ExternalHelperd;
struct Logger;
struct DatagramListener;
struct InternodeSyncer;
struct AckManager;
struct Flusher;
struct Node;
struct NodeStoreEx;
struct TargetMapper;
struct MirrorBuddyGroupMapper;
struct TargetStateStore;
struct NoAllocBufferStore;
struct AcknowledgmentStore;
struct NetFilter;
struct InodeRefStore;
struct StatFsCache;



struct App;
typedef struct App App;


extern void App_init(App* this, MountConfig* mountConfig);
extern void App_uninit(App* this);

extern int App_run(App* this);
extern void App_stop(App* this);

extern bool __App_initDataObjects(App* this, MountConfig* mountConfig);
extern bool __App_initInodeOperations(App* this);
extern bool __App_initLocalNodeInfo(App* this);
extern bool __App_initComponents(App* this);
extern void __App_startComponents(App* this);
extern void __App_stopComponents(App* this);
extern void __App_joinComponents(App* this);
extern void __App_waitForComponentTermination(App* this, Thread* component);

extern void __App_logInfos(App* this);

extern bool __App_mountServerCheck(App* this);

extern bool App_findAllowedInterfaces(App* this, NicAddressList* nicList);
extern void App_findAllowedRDMAInterfaces(App* this, NicAddressList* nicList, NicAddressList* rdmaNicList);

// external getters & setters
extern const char* App_getVersionStr(void);
extern void App_updateLocalInterfaces(App* app, NicAddressList* nicList);
extern void App_cloneLocalNicList(App* this, NicAddressList* nicList);
extern void App_cloneLocalRDMANicList(App* this, NicAddressList* rdmaNicList);

// inliners
static inline struct Logger* App_getLogger(App* this);
static inline struct ExternalHelperd* App_getHelperd(App* this);
static inline struct Config* App_getConfig(App* this);
static inline struct MountConfig* App_getMountConfig(App* this);
static inline struct NetFilter* App_getNetFilter(App* this);
static inline struct NetFilter* App_getTcpOnlyFilter(App* this);
static inline NicAddressList* App_getLocalRDMANicListLocked(App* this);
/**
 * Called when access to the nicList is required but doesn't
 * want the overhead of a clone operation. This locks the internel nicListMutex.
 * App_unlockNicList must later be invoked.
 */
static inline void App_lockNicList(App* this);
/**
 * Release the lock on nicListMutex acquired by App_lockNicList.
 */
static inline void App_unlockNicList(App* this);
static inline UInt16List* App_getPreferredStorageTargets(App* this);
static inline UInt16List* App_getPreferredMetaNodes(App* this);
static inline struct Node* App_getLocalNode(App* this);
static inline struct NodeStoreEx* App_getMgmtNodes(App* this);
static inline struct NodeStoreEx* App_getMetaNodes(App* this);
static inline struct NodeStoreEx* App_getStorageNodes(App* this);
static inline struct TargetMapper* App_getTargetMapper(App* this);
static inline struct MirrorBuddyGroupMapper* App_getStorageBuddyGroupMapper(App* this);
static inline struct MirrorBuddyGroupMapper* App_getMetaBuddyGroupMapper(App* this);
static inline struct TargetStateStore* App_getTargetStateStore(App* this);
static inline struct TargetStateStore* App_getMetaStateStore(App* this);
static inline struct NoAllocBufferStore* App_getCacheBufStore(App* this);
static inline struct NoAllocBufferStore* App_getPathBufStore(App* this);
static inline struct NoAllocBufferStore* App_getMsgBufStore(App* this);
static inline struct AcknowledgmentStore* App_getAckStore(App* this);
static inline struct InodeRefStore* App_getInodeRefStore(App* this);
static inline struct StatFsCache* App_getStatFsCache(App* this);
static inline struct DatagramListener* App_getDatagramListener(App* this);
static inline struct InternodeSyncer* App_getInternodeSyncer(App* this);
static inline struct AckManager* App_getAckManager(App* this);
static inline AtomicInt* App_getLockAckAtomicCounter(App* this);
static inline bool App_getConnRetriesEnabled(App* this);
static inline void App_setConnRetriesEnabled(App* this, bool connRetriesEnabled);
static inline bool App_getNetBenchModeEnabled(App* this);
static inline void App_setNetBenchModeEnabled(App* this, bool netBenchModeEnabled);

static inline struct inode_operations* App_getFileInodeOps(App* this);
static inline struct inode_operations* App_getSymlinkInodeOps(App* this);
static inline struct inode_operations* App_getDirInodeOps(App* this);
static inline struct inode_operations* App_getSpecialInodeOps(App* this);

#ifdef BEEGFS_DEBUG
static inline size_t App_getNumRPCs(App* this);
static inline void App_incNumRPCs(App* this);
static inline size_t App_getNumRemoteReads(App* this);
static inline void App_incNumRemoteReads(App* this);
static inline size_t App_getNumRemoteWrites(App* this);
static inline void App_incNumRemoteWrites(App* this);
#endif // BEEGFS_DEBUG


struct App
{
   int appResult;
   MountConfig* mountConfig;

   struct Config*  cfg;
   struct ExternalHelperd* helperd;
   struct Logger*  logger;

   struct NetFilter* netFilter; // empty filter means "all nets allowed"
   struct NetFilter* tcpOnlyFilter; // for IPs which allow only plain TCP (no RDMA etc)
   StrCpyList allowedInterfaces; // empty list means "all interfaces accepted"
   StrCpyList allowedRDMAInterfaces; // empty list means "all interfaces eligible"
   UInt16List preferredMetaNodes; // empty list means "no preferred nodes => use any"
   UInt16List preferredStorageTargets; // empty list means "no preferred nodes => use any"
   // rdmaNicList contains the addresses of specific RDMA NICs to use for outbound RDMA.
   // This is only populated when the configuration specifies a list of interfaces. If this
   // list is empty, any RDMA NIC on the client may be used for outbound RDMA.
   // allowedRDMAInterfaces contains the interface names used to populate this list.
   NicAddressList rdmaNicList;
   Mutex nicListMutex;

   struct Node* localNode;
   struct NodeStoreEx* mgmtNodes;
   struct NodeStoreEx* metaNodes;
   struct NodeStoreEx* storageNodes;

   struct TargetMapper* targetMapper;
   struct MirrorBuddyGroupMapper* storageBuddyGroupMapper;
   struct MirrorBuddyGroupMapper* metaBuddyGroupMapper;
   struct TargetStateStore* targetStateStore; // map storage targets IDs to a state
   struct TargetStateStore* metaStateStore; // map mds targets (i.e. nodeIDs) to a state

   struct NoAllocBufferStore* cacheBufStore; // for buffered cache mode
   struct NoAllocBufferStore* pathBufStore; // for dentry path lookups
   struct NoAllocBufferStore* msgBufStore; // for MessagingTk request/response
   struct AcknowledgmentStore* ackStore;
   struct InodeRefStore* inodeRefStore;
   struct StatFsCache* statfsCache;

   struct DatagramListener* dgramListener;
   struct InternodeSyncer* internodeSyncer;
   struct AckManager* ackManager;
   struct Flusher* flusher;

   AtomicInt lockAckAtomicCounter; // used by remoting to generate unique lockAckIDs
   volatile bool connRetriesEnabled; // changed at umount and via procfs
   bool netBenchModeEnabled; // changed via procfs to disable server-side disk read/write

   // Inode operations. Since the members of the structs depend on runtime config opts, we need
   // one copy of each struct per App object.
   struct inode_operations* fileInodeOps;
   struct inode_operations* symlinkInodeOps;
   struct inode_operations* dirInodeOps;
   struct inode_operations* specialInodeOps;

#ifdef BEEGFS_DEBUG
   Mutex debugCounterMutex; // this is the closed tree, so we don't have atomics here (but doesn't
      // matter since this is debug info and not performance critical)

   size_t numRPCs;
   size_t numRemoteReads;
   size_t numRemoteWrites;
#endif // BEEGFS_DEBUG

};


struct Logger* App_getLogger(App* this)
{
   return this->logger;
}

struct Config* App_getConfig(App* this)
{
   return this->cfg;
}

struct MountConfig* App_getMountConfig(App* this)
{
   return this->mountConfig;
}

struct ExternalHelperd* App_getHelperd(App* this)
{
   return this->helperd;
}

struct NetFilter* App_getNetFilter(App* this)
{
   return this->netFilter;
}

struct NetFilter* App_getTcpOnlyFilter(App* this)
{
   return this->tcpOnlyFilter;
}

UInt16List* App_getPreferredMetaNodes(App* this)
{
   return &this->preferredMetaNodes;
}

UInt16List* App_getPreferredStorageTargets(App* this)
{
   return &this->preferredStorageTargets;
}

void App_lockNicList(App* this)
{
   Mutex_lock(&this->nicListMutex); // L O C K
}

void App_unlockNicList(App* this)
{
   Mutex_unlock(&this->nicListMutex); // U N L O C K
}

NicAddressList* App_getLocalRDMANicListLocked(App* this)
{
   return &this->rdmaNicList;
}

struct Node* App_getLocalNode(App* this)
{
   return this->localNode;
}

struct NodeStoreEx* App_getMgmtNodes(App* this)
{
   return this->mgmtNodes;
}

struct NodeStoreEx* App_getMetaNodes(App* this)
{
   return this->metaNodes;
}

struct NodeStoreEx* App_getStorageNodes(App* this)
{
   return this->storageNodes;
}

struct TargetMapper* App_getTargetMapper(App* this)
{
   return this->targetMapper;
}

struct MirrorBuddyGroupMapper* App_getStorageBuddyGroupMapper(App* this)
{
   return this->storageBuddyGroupMapper;
}

struct MirrorBuddyGroupMapper* App_getMetaBuddyGroupMapper(App* this)
{
   return this->metaBuddyGroupMapper;
}

struct TargetStateStore* App_getTargetStateStore(App* this)
{
   return this->targetStateStore;
}

struct TargetStateStore* App_getMetaStateStore(App* this)
{
   return this->metaStateStore;
}

struct NoAllocBufferStore* App_getCacheBufStore(App* this)
{
   return this->cacheBufStore;
}

struct NoAllocBufferStore* App_getPathBufStore(App* this)
{
   return this->pathBufStore;
}

struct NoAllocBufferStore* App_getMsgBufStore(App* this)
{
   return this->msgBufStore;
}

struct AcknowledgmentStore* App_getAckStore(App* this)
{
   return this->ackStore;
}

struct InodeRefStore* App_getInodeRefStore(App* this)
{
   return this->inodeRefStore;
}

struct StatFsCache* App_getStatFsCache(App* this)
{
   return this->statfsCache;
}

struct DatagramListener* App_getDatagramListener(App* this)
{
   return this->dgramListener;
}

struct InternodeSyncer* App_getInternodeSyncer(App* this)
{
   return this->internodeSyncer;
}

struct AckManager* App_getAckManager(App* this)
{
   return this->ackManager;
}

AtomicInt* App_getLockAckAtomicCounter(App* this)
{
   return &this->lockAckAtomicCounter;
}

bool App_getConnRetriesEnabled(App* this)
{
   return this->connRetriesEnabled;
}

void App_setConnRetriesEnabled(App* this, bool connRetriesEnabled)
{
   this->connRetriesEnabled = connRetriesEnabled;
}

bool App_getNetBenchModeEnabled(App* this)
{
   return this->netBenchModeEnabled;
}

void App_setNetBenchModeEnabled(App* this, bool netBenchModeEnabled)
{
   this->netBenchModeEnabled = netBenchModeEnabled;
}

struct inode_operations* App_getFileInodeOps(App* this)
{
   return this->fileInodeOps;
}

struct inode_operations* App_getSymlinkInodeOps(App* this)
{
   return this->symlinkInodeOps;
}

struct inode_operations* App_getDirInodeOps(App* this)
{
   return this->dirInodeOps;
}

struct inode_operations* App_getSpecialInodeOps(App* this)
{
   return this->specialInodeOps;
}

#ifdef BEEGFS_DEBUG

   size_t App_getNumRPCs(App* this)
   {
      return this->numRPCs;
   }

   void App_incNumRPCs(App* this)
   {
      Mutex_lock(&this->debugCounterMutex);
      this->numRPCs++;
      Mutex_unlock(&this->debugCounterMutex);
   }

   size_t App_getNumRemoteReads(App* this)
   {
      return this->numRemoteReads;
   }

   void App_incNumRemoteReads(App* this)
   {
      Mutex_lock(&this->debugCounterMutex);
      this->numRemoteReads++;
      Mutex_unlock(&this->debugCounterMutex);
   }

   size_t App_getNumRemoteWrites(App* this)
   {
      return this->numRemoteWrites;
   }

   void App_incNumRemoteWrites(App* this)
   {
      Mutex_lock(&this->debugCounterMutex);
      this->numRemoteWrites++;
      Mutex_unlock(&this->debugCounterMutex);
   }

#else // BEEGFS_DEBUG

   #define App_incNumRPCs(this)
   #define App_incNumRemoteReads(this)
   #define App_incNumRemoteWrites(this)

#endif // BEEGFS_DEBUG


#endif /*APP_H_*/
