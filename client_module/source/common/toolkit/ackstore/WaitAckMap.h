#ifndef WAITACKMAP_H_
#define WAITACKMAP_H_

#include <common/nodes/Node.h>
#include <common/toolkit/tree/PointerRBTree.h>
#include <common/toolkit/tree/PointerRBTreeIter.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>


struct WaitAckNotification;
typedef struct WaitAckNotification WaitAckNotification;

struct WaitAck;
typedef struct WaitAck WaitAck;


struct WaitAckMapElem;
typedef struct WaitAckMapElem WaitAckMapElem;
struct WaitAckMap;
typedef struct WaitAckMap WaitAckMap;

struct WaitAckMapIter; // forward declaration of the iterator


static inline void WaitAckNotification_init(WaitAckNotification* this);
static inline void WaitAckNotification_uninit(WaitAckNotification* this);

static inline void WaitAck_init(WaitAck* this, char* ackID, void* privateData);


static inline void WaitAckMap_init(WaitAckMap* this);
static inline void WaitAckMap_uninit(WaitAckMap* this);

static inline bool WaitAckMap_insert(WaitAckMap* this, char* newKey,
   WaitAck* newValue);
static inline bool WaitAckMap_erase(WaitAckMap* this, const char* eraseKey);
static inline size_t WaitAckMap_length(WaitAckMap* this);

static inline void WaitAckMap_clear(WaitAckMap* this);

extern struct WaitAckMapIter WaitAckMap_find(WaitAckMap* this, const char* searchKey);
extern struct WaitAckMapIter WaitAckMap_begin(WaitAckMap* this);

extern int compareWaitAckMapElems(const void* key1, const void* key2);


struct WaitAckNotification
{
   Mutex waitAcksMutex; // this mutex also syncs access to the waitMap/receivedMap during the
      // wait phase (which is between registration and deregistration)
   Condition waitAcksCompleteCond; // in case all WaitAcks have been received
};

struct WaitAck
{
      char* ackID;
      void* privateData; // caller's private data
};



struct WaitAckMapElem
{
   RBTreeElem rbTreeElem;
};

struct WaitAckMap
{
   RBTree rbTree;
};


void WaitAckNotification_init(WaitAckNotification* this)
{
   Mutex_init(&this->waitAcksMutex);
   Condition_init(&this->waitAcksCompleteCond);
}

void WaitAckNotification_uninit(WaitAckNotification* this)
{
   Mutex_uninit(&this->waitAcksMutex);
}

/**
 * @param ackID is not copied, so don't free or touch it while you use this object
 * @param privateData any private data that helps the caller to identify to ack later
 */
void WaitAck_init(WaitAck* this, char* ackID, void* privateData)
{
   this->ackID = ackID;
   this->privateData = privateData;
}

void WaitAckMap_init(WaitAckMap* this)
{
   PointerRBTree_init( (RBTree*)this, compareWaitAckMapElems);
}

void WaitAckMap_uninit(WaitAckMap* this)
{
   PointerRBTree_uninit( (RBTree*)this);
}


bool WaitAckMap_insert(WaitAckMap* this, char* newKey, WaitAck* newValue)
{
   bool insRes;

   insRes = PointerRBTree_insert( (RBTree*)this, newKey, newValue);
   if(!insRes)
   {
      // not inserted because the key already existed
   }

   return insRes;
}

bool WaitAckMap_erase(WaitAckMap* this, const char* eraseKey)
{
   return PointerRBTree_erase( (RBTree*)this, eraseKey);
}

size_t WaitAckMap_length(WaitAckMap* this)
{
   return PointerRBTree_length( (RBTree*)this);
}


void WaitAckMap_clear(WaitAckMap* this)
{
   PointerRBTree_clear(&this->rbTree);
}


#endif /* WAITACKMAP_H_ */
