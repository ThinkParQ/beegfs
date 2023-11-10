#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <app/config/Config.h>
#include <app/log/Logger.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/TargetMapper.h>
#include <common/storage/FileEvent.h>
#include <common/toolkit/list/PointerListIter.h>
#include <common/toolkit/MetadataTk.h>
#include <common/threading/AtomicInt.h>
#include <common/threading/Thread.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/TargetStateStore.h>
#include <components/DatagramListener.h>
#include <net/filesystem/RemotingIOInfo.h>
#include <nodes/NodeStoreEx.h>
#include <toolkit/ExternalHelperd.h>


/*
 * note: Delayed...Entry-queues management is integrated into InternodeSyncer, because it needs
 * direct access to the queues via iterators and relies on special access patterns, e.g.
 * it must be the only one removing entries from the queue (to keep iterators valid).
 */


// forward declarations...

struct InternodeSyncer;
typedef struct InternodeSyncer InternodeSyncer;

struct DelayedCloseEntry;
typedef struct DelayedCloseEntry DelayedCloseEntry;
struct DelayedEntryUnlockEntry;
typedef struct DelayedEntryUnlockEntry DelayedEntryUnlockEntry;
struct DelayedRangeUnlockEntry;
typedef struct DelayedRangeUnlockEntry DelayedRangeUnlockEntry;


extern void InternodeSyncer_init(InternodeSyncer* this, App* app);
extern InternodeSyncer* InternodeSyncer_construct(App* app);
extern void InternodeSyncer_uninit(InternodeSyncer* this);
extern void InternodeSyncer_destruct(InternodeSyncer* this);

extern bool InternodeSyncer_waitForMgmtInit(InternodeSyncer* this, int timeoutMS);

extern void InternodeSyncer_delayedCloseAdd(InternodeSyncer* this, const EntryInfo* entryInfo,
   const RemotingIOInfo* ioInfo, struct FileEvent* event);
extern void InternodeSyncer_delayedEntryUnlockAdd(InternodeSyncer* this,
   const EntryInfo* entryInfo, const RemotingIOInfo* ioInfo, int64_t clientFD);
extern void InternodeSyncer_delayedRangeUnlockAdd(InternodeSyncer* this,
   const EntryInfo* entryInfo, const RemotingIOInfo* ioInfo, int ownerPID);


extern void _InternodeSyncer_requestLoop(InternodeSyncer* this);


extern void __InternodeSyncer_run(Thread* this);

extern void __InternodeSyncer_signalMgmtInitDone(InternodeSyncer* this);
extern void __InternodeSyncer_mgmtInit(InternodeSyncer* this);
extern bool __InternodeSyncer_waitForMgmtHeartbeat(InternodeSyncer* this);

extern bool __InternodeSyncer_registerNode(InternodeSyncer* this);
extern void __InternodeSyncer_reregisterNode(InternodeSyncer* this);
extern bool __InternodeSyncer_unregisterNode(InternodeSyncer* this);

extern void __InternodeSyncer_downloadAndSyncNodes(InternodeSyncer* this);
extern void __InternodeSyncer_printSyncResults(InternodeSyncer* this, NodeType nodeType,
   NumNodeIDList* addedNodes, NumNodeIDList* removedNodes);
extern void __InternodeSyncer_downloadAndSyncTargetMappings(InternodeSyncer* this);

extern void __InternodeSyncer_delayedCloseComm(InternodeSyncer* this);
extern void __InternodeSyncer_delayedEntryUnlockComm(InternodeSyncer* this);
extern void __InternodeSyncer_delayedRangeUnlockComm(InternodeSyncer* this);

extern void __InternodeSyncer_delayedClosePrepareRemoting(InternodeSyncer* this,
   DelayedCloseEntry* closeEntry, EntryInfo** outEntryInfo, RemotingIOInfo* outIOInfo);
extern void __InternodeSyncer_delayedEntryUnlockPrepareRemoting(InternodeSyncer* this,
   DelayedEntryUnlockEntry* closeEntry, EntryInfo** outEntryInfo, RemotingIOInfo* outIOInfo);
extern void __InternodeSyncer_delayedRangeUnlockPrepareRemoting(InternodeSyncer* this,
   DelayedRangeUnlockEntry* closeEntry, EntryInfo** outEntryInfo, RemotingIOInfo* outIOInfo);

extern void __InternodeSyncer_delayedCloseFreeEntry(InternodeSyncer* this,
   DelayedCloseEntry* closeEntry);
extern void __InternodeSyncer_delayedEntryUnlockFreeEntry(InternodeSyncer* this,
   DelayedEntryUnlockEntry* closeEntry);
extern void __InternodeSyncer_delayedRangeUnlockFreeEntry(InternodeSyncer* this,
   DelayedRangeUnlockEntry* closeEntry);

extern void __InternodeSyncer_dropIdleConns(InternodeSyncer* this);
extern unsigned __InternodeSyncer_dropIdleConnsByStore(InternodeSyncer* this, NodeStoreEx* nodes);

extern void __InternodeSyncer_updateTargetStatesAndBuddyGroups(InternodeSyncer* this,
   NodeType nodeType);
extern bool __InternodeSyncer_checkNetwork(InternodeSyncer* this);

// getters & setters
static inline void InternodeSyncer_setForceTargetStatesUpdate(InternodeSyncer* this);
static inline bool InternodeSyncer_getAndResetForceTargetStatesUpdate(InternodeSyncer* this);
extern size_t InternodeSyncer_getDelayedCloseQueueSize(InternodeSyncer* this);
extern size_t InternodeSyncer_getDelayedEntryUnlockQueueSize(InternodeSyncer* this);
extern size_t InternodeSyncer_getDelayedRangeUnlockQueueSize(InternodeSyncer* this);


struct DelayedCloseEntry
{
   Time ageT; // time when this entry was created (to compute entry age)

   EntryInfo entryInfo;

   // fields for RemotingIOInfo
   const char* fileHandleID;
   unsigned accessFlags; // OPENFILE_ACCESS_... flags

   bool needsAppendLockCleanup;
   AtomicInt maxUsedTargetIndex; // (used as a reference in ioInfo)

   bool hasEvent;
   struct FileEvent event;
};

struct DelayedEntryUnlockEntry
{
   Time ageT; // time when this entry was created (to compute entry age)

   EntryInfo entryInfo;

   // fields for RemotingIOInfo
   const char* fileHandleID;

   int64_t clientFD;
};

struct DelayedRangeUnlockEntry
{
   Time ageT; // time when this entry was created (to compute entry age)

   EntryInfo entryInfo;

   // fields for RemotingIOInfo
   const char* fileHandleID;

   int ownerPID;
};


struct InternodeSyncer
{
   Thread thread; // base class

   App* app;
   Config* cfg;

   DatagramListener* dgramLis;
   ExternalHelperd* helperd;

   NodeStoreEx* mgmtNodes;
   NodeStoreEx* metaNodes;
   NodeStoreEx* storageNodes;

   bool mgmtInitDone;
   Mutex mgmtInitDoneMutex;
   Condition mgmtInitDoneCond; // signaled when init is done (doesn't mean it was successful)

   bool nodeRegistered; // true if the mgmt host ack'ed our heartbeat

   bool forceTargetStatesUpdate; // force an update of target states
   Time lastSuccessfulTargetStatesUpdateT;
   unsigned targetOfflineTimeoutMS;

   Mutex delayedCloseMutex; // for delayedCloseQueue
   PointerList delayedCloseQueue; /* remove from head, add to tail (important, because we rely on
      validity of an iterator even when a new entry was added) */
   Mutex delayedEntryUnlockMutex; // for delayedEntryUnlockQueue
   PointerList delayedEntryUnlockQueue; /* remove from head, add to tail (important, because we
      rely on validity of an iterator even when a new entry was added) */
   Mutex delayedRangeUnlockMutex; // for delayedRangeUnlockQueue
   PointerList delayedRangeUnlockQueue; /* remove from head, add to tail (important, because we
      rely on validity of an iterator even when a new entry was added) */
   Mutex forceTargetStatesUpdateMutex; // for forceTargetStates
};


void InternodeSyncer_setForceTargetStatesUpdate(InternodeSyncer* this)
{
   Mutex_lock(&this->forceTargetStatesUpdateMutex); // L O C K

   this->forceTargetStatesUpdate = true;

   Mutex_unlock(&this->forceTargetStatesUpdateMutex); // U N L O C K
}

bool InternodeSyncer_getAndResetForceTargetStatesUpdate(InternodeSyncer* this)
{
   bool retVal;

   Mutex_lock(&this->forceTargetStatesUpdateMutex); // L O C K

   retVal = this->forceTargetStatesUpdate;
   this->forceTargetStatesUpdate = false;

   Mutex_unlock(&this->forceTargetStatesUpdateMutex); // U N L O C K

   return retVal;
}

#endif /*INTERNODESYNCER_H_*/
