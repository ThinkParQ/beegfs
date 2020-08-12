#include <app/App.h>
#include <common/net/message/control/AckMsgEx.h>
#include <common/toolkit/Time.h>
#include "AckManager.h"


#define ACKMANAGER_MSGBUF_LEN     4096


void AckManager_init(AckManager* this, App* app)
{
   Thread_init(&this->thread, BEEGFS_THREAD_NAME_PREFIX_STR "AckMgr", __AckManager_run);

   this->app = app;
   this->cfg = App_getConfig(app);

   this->ackMsgBuf = vmalloc(ACKMANAGER_MSGBUF_LEN);

   Mutex_init(&this->ackQueueMutex);
   Condition_init(&this->ackQueueAddedCond);
   PointerList_init(&this->ackQueue);
}

struct AckManager* AckManager_construct(App* app)
{
   struct AckManager* this = (AckManager*)os_kmalloc(sizeof(*this) );

   AckManager_init(this, app);

   return this;
}

void AckManager_uninit(AckManager* this)
{
   PointerListIter iter;

   // free ackQ elements

   PointerListIter_init(&iter, &this->ackQueue);

   while(!PointerListIter_end(&iter) )
   {
      __AckManager_freeQueueEntry(this, (AckQueueEntry*)PointerListIter_value(&iter) );
      PointerListIter_next(&iter);
   }

   PointerList_uninit(&this->ackQueue);
   Mutex_uninit(&this->ackQueueMutex);

   vfree(this->ackMsgBuf);

   Thread_uninit( (Thread*)this);
}

void AckManager_destruct(AckManager* this)
{
   AckManager_uninit(this);

   kfree(this);
}

void _AckManager_requestLoop(AckManager* this)
{
   int sleepTimeMS = 2500;

   Thread* thisThread = (Thread*)this;

   while(!Thread_getSelfTerminate(thisThread) )
   {
      // wait for new queue entries

      Mutex_lock(&this->ackQueueMutex); // L O C K

      if(!PointerList_length(&this->ackQueue) )
      {
         Condition_timedwaitInterruptible(
            &this->ackQueueAddedCond, &this->ackQueueMutex, sleepTimeMS);
      }

      Mutex_unlock(&this->ackQueueMutex); // U N L O C K


      // process new queue entries (if any)

      __AckManager_processAckQueue(this);
   }

}


void __AckManager_run(Thread* this)
{
   AckManager* thisCast = (AckManager*)this;

   const char* logContext = "AckManager (run)";
   Logger* log = App_getLogger(thisCast->app);


   _AckManager_requestLoop(thisCast);

   Logger_log(log, 4, logContext, "Component stopped.");
}

/**
 * Send ack for all enqueued entries.
 */
void __AckManager_processAckQueue(AckManager* this)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "Queue processing";

   const int maxNumCommRetries = 1; /* note: we really don't want to wait too long in this method
      because time would run out for other acks then, so only a single retry allowed */

   PointerListIter iter;

   Mutex_lock(&this->ackQueueMutex); // L O C K

   PointerListIter_init(&iter, &this->ackQueue);

   while(!PointerListIter_end(&iter) && !Thread_getSelfTerminate( (Thread*)this) )
   {
      AckQueueEntry* currentAck = PointerListIter_value(&iter);

      NodeStoreEx* metaNodes = App_getMetaNodes(this->app);
      Node* node;
      AckMsgEx msg;
      unsigned msgLen;
      bool serialRes;
      NodeConnPool* connPool;
      int currentRetryNum;
      Socket* sock;
      ssize_t sendRes = 0;
      bool removeAllNextByNode = false;

      node = NodeStoreEx_referenceNode(metaNodes, currentAck->metaNodeID);
      if(unlikely(!node) )
      { // node not found in store
         Logger_logFormatted(log, Log_DEBUG, logContext, "Metadata node no longer exists: %hu",
            currentAck->metaNodeID.value);

         goto remove;
      }

      connPool = Node_getConnPool(node);

      AckMsgEx_initFromValue(&msg, currentAck->ackID);
      msgLen = NetMessage_getMsgLength( (NetMessage*)&msg);

      serialRes = NetMessage_serialize( (NetMessage*)&msg, this->ackMsgBuf, ACKMANAGER_MSGBUF_LEN);
      if(unlikely(!serialRes) )
      { // serialization failed
         Logger_logFormatted(log, Log_CRITICAL, logContext,
            "BUG(?): Unable to serialize ack msg for metadata node: %hu (ack: %s)",
            currentAck->metaNodeID.value, currentAck->ackID);

         goto release;
      }

      for(currentRetryNum=0; currentRetryNum <= maxNumCommRetries; currentRetryNum++)
      {
         if(currentRetryNum == 1)
         { // inform user about retry (only on first retry to not spam the log)
            Logger_logFormatted(log, Log_NOTICE, logContext,
               "Retrying communication with metadata node: %hu", currentAck->metaNodeID.value);
         }

         // unlock, so that more entries can be added to the queue during remoting without waiting

         Mutex_unlock(&this->ackQueueMutex); // U N L O C K

         // connect & communicate

         sock = NodeConnPool_acquireStreamSocket(connPool);

         if(likely(sock) )
         { // send msg
            sendRes = Socket_send(sock, this->ackMsgBuf, msgLen, 0);

            if(unlikely(sendRes != (ssize_t) msgLen) )
            { // comm error => invalidate conn
               NodeConnPool_invalidateStreamSocket(connPool, sock);
            }
            else
               NodeConnPool_releaseStreamSocket(connPool, sock);
         }

         Mutex_lock(&this->ackQueueMutex); // R E L O C K

         // check comm errors

         if(unlikely(!sock || (sendRes != (ssize_t) msgLen) ) )
         { // no connection or communication error
            Logger_logFormatted(log, Log_NOTICE, logContext,
               "Communication with metadata node failed: %hu", currentAck->metaNodeID.value);

            removeAllNextByNode = true; // (only effective if no more retries)

            continue;
         }

         removeAllNextByNode = false;

         break; // communication succeeded => we're done with this retry-loop

      } // end of comm retry for-loop


      if(removeAllNextByNode)
      { // comm with current node failed => remove all following entries with this nodeID

         // note: only following entries, because current entry will be free'd below anyways.

         PointerListIter iterNext = iter;
         PointerListIter_next(&iterNext);

         __AckManager_removeQueueEntriesByNode(this, currentAck->metaNodeID, iterNext);
      }

   release:
      Node_put(node);

   remove:
      __AckManager_freeQueueEntry(this, currentAck);
      iter = PointerListIter_remove(&iter);
   } // end of while(!list_end) loop


   Mutex_unlock(&this->ackQueueMutex); // U N L O C K
}

/**
 * Frees/uninits all sub-fields and kfrees the closeEntry itself (but does not remove it from the
 * queue).
 */
void __AckManager_freeQueueEntry(AckManager* this, AckQueueEntry* ackEntry)
{
   kfree(ackEntry->ackID);

   kfree(ackEntry);
}

/**
 * Free all entries in the queue for the given nodeID.
 * (Typcally used when communication with a certain node failed.)
 *
 * @param iter starting point for removal
 */
void __AckManager_removeQueueEntriesByNode(AckManager* this, NumNodeID nodeID,
   PointerListIter iter)
{
   while(!PointerListIter_end(&iter) )
   {
      AckQueueEntry* currentAck = PointerListIter_value(&iter);

      if(NumNodeID_compare(&nodeID, &currentAck->metaNodeID))
      { // nodeID matches
         __AckManager_freeQueueEntry(this, currentAck);

         iter = PointerListIter_remove(&iter);
      }
      else
         PointerListIter_next(&iter);
   }

}

/**
 * Add an ack that should reliably (i.e. not via UDP) be transmitted to the given meta server.
 *
 * @param metaNodeID will be copied
 * @param ackID will be copied
 */
void AckManager_addAckToQueue(AckManager* this, NumNodeID metaNodeID, const char* ackID)
{
   AckQueueEntry* newEntry = (AckQueueEntry*)os_kmalloc(sizeof(*newEntry) );

   Time_init(&newEntry->ageT);

   newEntry->metaNodeID = metaNodeID;
   newEntry->ackID = StringTk_strDup(ackID);

   // add new entry to queue and wake up AckManager thread...

   Mutex_lock(&this->ackQueueMutex); // L O C K

   PointerList_append(&this->ackQueue, newEntry);

   Condition_signal(&this->ackQueueAddedCond);

   Mutex_unlock(&this->ackQueueMutex); // U N L O C K
}


size_t AckManager_getAckQueueSize(AckManager* this)
{
   size_t retVal;

   Mutex_lock(&this->ackQueueMutex); // L O C K

   retVal = PointerList_length(&this->ackQueue);

   Mutex_unlock(&this->ackQueueMutex); // U N L O C K

   return retVal;
}

