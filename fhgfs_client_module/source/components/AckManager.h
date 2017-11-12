#ifndef ACKMANAGER_H_
#define ACKMANAGER_H_

#include <app/config/Config.h>
#include <app/log/Logger.h>
#include <common/toolkit/list/PointerListIter.h>
#include <components/AckManager.h>
#include <common/threading/Thread.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/net/message/NetMessage.h>
#include <nodes/NodeStoreEx.h>
#include <toolkit/ExternalHelperd.h>


/*
 * note: AckEntry-queues management is integrated into AckManager, because it needs
 * direct access to the queues via iterators and relies on special access patterns, e.g.
 * it must be the only one removing entries from the queue (to keep iterators valid).
 */

// forward declarations...

struct AckQueueEntry;
typedef struct AckQueueEntry AckQueueEntry;

struct AckManager;
typedef struct AckManager AckManager;


extern void AckManager_init(AckManager* this, App* app);
extern AckManager* AckManager_construct(App* app);
extern void AckManager_uninit(AckManager* this);
extern void AckManager_destruct(AckManager* this);

extern void _AckManager_requestLoop(AckManager* this);

extern void __AckManager_run(Thread* this);
extern void __AckManager_processAckQueue(AckManager* this);
extern void __AckManager_freeQueueEntry(AckManager* this, AckQueueEntry* ackEntry);
extern void __AckManager_removeQueueEntriesByNode(AckManager* this, NumNodeID nodeID,
   PointerListIter iter);
extern void AckManager_addAckToQueue(AckManager* this, NumNodeID metaNodeID, const char* ackID);

// getters & setters
extern size_t AckManager_getAckQueueSize(AckManager* this);


struct AckQueueEntry
{
   Time ageT; // time when this entry was created (to compute entry age)

   NumNodeID metaNodeID;
   const char* ackID;
};


struct AckManager
{
   Thread thread; // base class

   App* app;
   Config* cfg;

   char* ackMsgBuf; // static buffer for message serialization

   Mutex ackQueueMutex; // for ackQueue
   Condition ackQueueAddedCond; // when entries are added to queue
   PointerList ackQueue; /* remove from head, add to tail (important, because we rely on
      validity of an iterator even when a new entry was added) */
};

#endif /* ACKMANAGER_H_ */
