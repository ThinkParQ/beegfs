#ifndef ACKSTOREMAP_H_
#define ACKSTOREMAP_H_

#include <common/toolkit/tree/PointerRBTree.h>
#include <common/toolkit/tree/PointerRBTreeIter.h>
#include "WaitAckMap.h"


struct AckStoreEntry;
typedef struct AckStoreEntry AckStoreEntry;


struct AckStoreMapElem;
typedef struct AckStoreMapElem AckStoreMapElem;
struct AckStoreMap;
typedef struct AckStoreMap AckStoreMap;

struct AckStoreMapIter; // forward declaration of the iterator


static inline void AckStoreEntry_init(AckStoreEntry* this, char* ackID,
   WaitAckMap* waitMap, WaitAckMap* receivedMap, WaitAckNotification* notifier);
static inline AckStoreEntry* AckStoreEntry_construct(char* ackID,
   WaitAckMap* waitMap, WaitAckMap* receivedMap, WaitAckNotification* notifier);
static inline void AckStoreEntry_destruct(AckStoreEntry* this);


static inline void AckStoreMap_init(AckStoreMap* this);
static inline void AckStoreMap_uninit(AckStoreMap* this);

static inline bool AckStoreMap_insert(AckStoreMap* this, char* newKey,
   AckStoreEntry* newValue);
static inline bool AckStoreMap_erase(AckStoreMap* this, const char* eraseKey);

extern struct AckStoreMapIter AckStoreMap_find(AckStoreMap* this, const char* searchKey);

extern int compareAckStoreMapElems(const void* key1, const void* key2);


struct AckStoreEntry
{
      char* ackID;

      WaitAckMap* waitMap; // ack will be removed from this map if it is received
      WaitAckMap* receivedMap; // ack will be added to this map if it is received

      WaitAckNotification* notifier;
};



struct AckStoreMapElem
{
   RBTreeElem rbTreeElem;
};

struct AckStoreMap
{
   RBTree rbTree;
};


void AckStoreEntry_init(AckStoreEntry* this, char* ackID,
   WaitAckMap* waitMap, WaitAckMap* receivedMap, WaitAckNotification* notifier)
{
   this->ackID = ackID;
   this->waitMap = waitMap;
   this->receivedMap = receivedMap;
   this->notifier = notifier;
}

AckStoreEntry* AckStoreEntry_construct(char* ackID,
   WaitAckMap* waitMap, WaitAckMap* receivedMap, WaitAckNotification* notifier)
{
   AckStoreEntry* this = (AckStoreEntry*)os_kmalloc(sizeof(*this) );

   AckStoreEntry_init(this, ackID, waitMap, receivedMap, notifier);

   return this;
}

void AckStoreEntry_destruct(AckStoreEntry* this)
{
   kfree(this);
}



void AckStoreMap_init(AckStoreMap* this)
{
   PointerRBTree_init( (RBTree*)this, compareAckStoreMapElems);
}

void AckStoreMap_uninit(AckStoreMap* this)
{
   PointerRBTree_uninit( (RBTree*)this);
}

bool AckStoreMap_insert(AckStoreMap* this, char* newKey, AckStoreEntry* newValue)
{
   bool insRes;

   insRes = PointerRBTree_insert( (RBTree*)this, newKey, newValue);
   if(!insRes)
   {
      // not inserted because the key already existed
   }

   return insRes;
}

bool AckStoreMap_erase(AckStoreMap* this, const char* eraseKey)
{
   return PointerRBTree_erase( (RBTree*)this, eraseKey);
}

#endif /* ACKSTOREMAP_H_ */
