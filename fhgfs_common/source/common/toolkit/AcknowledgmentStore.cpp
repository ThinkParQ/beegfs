#include "AcknowledgmentStore.h"

/**
 * Note: This method does not lock the notifier->waitAcksMutex, because this is typically
 * called from a context that does not yet require syncing.
 * 
 * @param waitAcks do not access this map until you called unregister (for thread-safety)
 * @param receivedAcks do not access this map until you called unregister (for thread-safety)
 */
void AcknowledgmentStore::registerWaitAcks(WaitAckMap* waitAcks, WaitAckMap* receivedAcks,
   WaitAckNotification* notifier)
{
   SafeMutexLock storeMutexLock(&mutex);

   AckStoreEntry newStoreEntry;
   
   for(WaitAckMapIter iter = waitAcks->begin(); iter != waitAcks->end(); iter++)
   {
      WaitAck* currentWaitAck = &iter->second;
      
      newStoreEntry.ackID = currentWaitAck->ackID;
      newStoreEntry.waitMap = waitAcks;
      newStoreEntry.receivedMap = receivedAcks;
      newStoreEntry.notifier = notifier;
      
      storeMap.insert(AckStoreMapVal(currentWaitAck->ackID, newStoreEntry) );
   }
   
   storeMutexLock.unlock();
}

void AcknowledgmentStore::unregisterWaitAcks(WaitAckMap* waitAcks)
{
   SafeMutexLock storeMutexLock(&mutex);

   for(WaitAckMapIter iter = waitAcks->begin(); iter != waitAcks->end(); iter++)
   {
      storeMap.erase(iter->first);
   }
   
   storeMutexLock.unlock();
}

void AcknowledgmentStore::receivedAck(std::string ackID)
{
   SafeMutexLock storeMutexLock(&mutex);

   AckStoreMapIter storeIter = storeMap.find(ackID);
   if(storeIter != storeMap.end() )
   { // ack entry exists in store
      AckStoreEntry* storeEntry = &storeIter->second;
      
      SafeMutexLock waitMutexLock(&storeEntry->notifier->waitAcksMutex);
      
      WaitAckMapIter waitIter = storeEntry->waitMap->find(ackID);
      if(waitIter != storeEntry->waitMap->end() )
      { // entry exists in waitMap => move to receivedMap
         
         storeEntry->receivedMap->insert(WaitAckMapVal(ackID, waitIter->second) );
         storeEntry->waitMap->erase(waitIter);

         if(storeEntry->waitMap->empty() )
         { // all acks received => notify
            WaitAckNotification* notifier = storeEntry->notifier;
            
            notifier->waitAcksCompleteCond.broadcast();
         }
      }

      waitMutexLock.unlock();
      
      storeMap.erase(storeIter);
   }
   
   storeMutexLock.unlock();
}


bool AcknowledgmentStore::waitForAckCompletion(WaitAckMap* waitAcks, WaitAckNotification* notifier,
   int timeoutMS)
{
   bool retVal;
   
   SafeMutexLock waitMutexLock(&notifier->waitAcksMutex);
   
   if(!waitAcks->empty() )
      notifier->waitAcksCompleteCond.timedwait(&notifier->waitAcksMutex, timeoutMS);
   
   retVal = waitAcks->empty();
   
   waitMutexLock.unlock();
   
   return retVal;
}
