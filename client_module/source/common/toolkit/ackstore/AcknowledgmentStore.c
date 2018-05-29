#include "AcknowledgmentStore.h"

/**
 * Note: This method does not lock the notifier->waitAcksMutex, because this is typically
 * called from a context that does not yet require syncing.
 *
 * @param waitAcks do not access this map until you called unregister (for thread-safety)
 * @param receivedAcks do not access this map until you called unregister (for thread-safety)
 * @param notifier used to wake up the thread waiting for acks
 */
void AcknowledgmentStore_registerWaitAcks(AcknowledgmentStore* this,
   WaitAckMap* waitAcks, WaitAckMap* receivedAcks, WaitAckNotification* notifier)
{
   WaitAckMapIter iter;

   Mutex_lock(&this->mutex);

   for(iter = WaitAckMap_begin(waitAcks); !WaitAckMapIter_end(&iter); WaitAckMapIter_next(&iter) )
   {
      WaitAck* currentWaitAck = WaitAckMapIter_value(&iter);

      AckStoreEntry* newStoreEntry = AckStoreEntry_construct(
         currentWaitAck->ackID, waitAcks, receivedAcks, notifier);

      AckStoreMap_insert(&this->storeMap, currentWaitAck->ackID, newStoreEntry);
   }

   Mutex_unlock(&this->mutex);
}

void AcknowledgmentStore_unregisterWaitAcks(AcknowledgmentStore* this, WaitAckMap* waitAcks)
{
   WaitAckMapIter iter;

   Mutex_lock(&this->mutex);

   for(iter = WaitAckMap_begin(waitAcks); !WaitAckMapIter_end(&iter); WaitAckMapIter_next(&iter) )
   {
      char* currentKey = WaitAckMapIter_key(&iter);
      AckStoreMapIter storeIter = AckStoreMap_find(&this->storeMap, currentKey);

      // free allocated entry and remove it from store map

      AckStoreEntry_destruct(AckStoreMapIter_value(&storeIter) );
      AckStoreMap_erase(&this->storeMap, currentKey);
   }

   Mutex_unlock(&this->mutex);
}

/**
 * @return true if someone was waiting for this ackID, false otherwise
 */
bool AcknowledgmentStore_receivedAck(AcknowledgmentStore* this, const char* ackID)
{
   bool retVal = false;
   AckStoreMapIter storeIter;

   Mutex_lock(&this->mutex); // L O C K (store)

   storeIter = AckStoreMap_find(&this->storeMap, ackID);
   if(!AckStoreMapIter_end(&storeIter) )
   { // ack entry exists in store
      WaitAckMapIter waitIter;
      AckStoreEntry* storeEntry = AckStoreMapIter_value(&storeIter);

      Mutex_lock(&storeEntry->notifier->waitAcksMutex); // L O C K (notifier)

      waitIter = WaitAckMap_find(storeEntry->waitMap, ackID);
      if(!WaitAckMapIter_end(&waitIter) )
      { // entry exists in waitMap => move to receivedMap
         char* waitAckID = WaitAckMapIter_key(&waitIter);
         WaitAck* waitAck = WaitAckMapIter_value(&waitIter);

         WaitAckMap_insert(storeEntry->receivedMap, waitAckID, waitAck);
         WaitAckMap_erase(storeEntry->waitMap, ackID);

         if(!WaitAckMap_length(storeEntry->waitMap) )
         { // all acks received => notify
            WaitAckNotification* notifier = storeEntry->notifier;

            Condition_broadcast(&notifier->waitAcksCompleteCond);
         }

         retVal = true;
      }

      Mutex_unlock(&storeEntry->notifier->waitAcksMutex); // U N L O C K (notifier)


      AckStoreEntry_destruct(storeEntry);
      AckStoreMap_erase(&this->storeMap, ackID);
   }

   Mutex_unlock(&this->mutex); // U N L O C K (store)

   return retVal;
}


bool AcknowledgmentStore_waitForAckCompletion(AcknowledgmentStore* this,
   WaitAckMap* waitAcks, WaitAckNotification* notifier, int timeoutMS)
{
   bool retVal;

   Mutex_lock(&notifier->waitAcksMutex);

   if(WaitAckMap_length(waitAcks) )
      Condition_timedwait(&notifier->waitAcksCompleteCond, &notifier->waitAcksMutex, timeoutMS);

   retVal = WaitAckMap_length(waitAcks) ? false : true;

   Mutex_unlock(&notifier->waitAcksMutex);

   return retVal;
}

