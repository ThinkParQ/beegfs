#ifndef ACKNOWLEDGMENTSTORE_H_
#define ACKNOWLEDGMENTSTORE_H_

#include <common/Common.h>
#include <common/nodes/Node.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include "AckStoreMap.h"
#include "AckStoreMapIter.h"
#include "WaitAckMap.h"
#include "WaitAckMapIter.h"


struct AcknowledgmentStore;
typedef struct AcknowledgmentStore AcknowledgmentStore;

static inline void AcknowledgmentStore_init(AcknowledgmentStore* this);
static inline AcknowledgmentStore* AcknowledgmentStore_construct(void);
static inline void AcknowledgmentStore_uninit(AcknowledgmentStore* this);
static inline void AcknowledgmentStore_destruct(AcknowledgmentStore* this);

extern void AcknowledgmentStore_registerWaitAcks(AcknowledgmentStore* this,
   WaitAckMap* waitAcks, WaitAckMap* receivedAcks, WaitAckNotification* notifier);
extern void AcknowledgmentStore_unregisterWaitAcks(AcknowledgmentStore* this, WaitAckMap* waitAcks);
extern bool AcknowledgmentStore_receivedAck(AcknowledgmentStore* this, const char* ackID);
extern bool AcknowledgmentStore_waitForAckCompletion(AcknowledgmentStore* this,
   WaitAckMap* waitAcks, WaitAckNotification* notifier, int timeoutMS);



struct AcknowledgmentStore
{
   AckStoreMap storeMap;
   Mutex mutex;
};


void AcknowledgmentStore_init(AcknowledgmentStore* this)
{
   AckStoreMap_init(&this->storeMap);
   Mutex_init(&this->mutex);
}

AcknowledgmentStore* AcknowledgmentStore_construct(void)
{
   AcknowledgmentStore* this = (AcknowledgmentStore*)os_kmalloc(sizeof(*this) );

   AcknowledgmentStore_init(this);

   return this;
}

void AcknowledgmentStore_uninit(AcknowledgmentStore* this)
{
   Mutex_uninit(&this->mutex);
   AckStoreMap_uninit(&this->storeMap);
}

void AcknowledgmentStore_destruct(AcknowledgmentStore* this)
{
   AcknowledgmentStore_uninit(this);

   kfree(this);
}


#endif /* ACKNOWLEDGMENTSTORE_H_ */
