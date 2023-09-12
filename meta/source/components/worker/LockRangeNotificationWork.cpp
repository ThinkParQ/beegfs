#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/control/AckMsg.h>
#include <common/net/message/session/locking/LockGrantedMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "LockRangeNotificationWork.h"

#include <mutex>

Mutex LockRangeNotificationWork::ackCounterMutex;
unsigned LockRangeNotificationWork::ackCounter = 0;


void LockRangeNotificationWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   /* note: this code is very similar to LockEntryNotificationWork, so if you change something here,
      you probably want to change it there, too. */

   const char* logContext = __func__;
   App* app = Program::getApp();
   Logger* logger = Logger::getLogger();

   Config* cfg = app->getConfig();
   AcknowledgmentStore* ackStore = app->getAckStore();
   DatagramListener* dgramLis = app->getDatagramListener();
   MetaStore* metaStore = app->getMetaStore();
   NodeStoreClients* clients = app->getClientNodes();
   NumNodeID localNodeID = app->getLocalNode().getNumID();

   // max total time is ackWaitMS * numRetries, defaults to 333ms * 15 => 5s
   int ackWaitSleepMS = cfg->getTuneLockGrantWaitMS();
   int numRetriesLeft = cfg->getTuneLockGrantNumRetries();

   WaitAckMap waitAcks;
   WaitAckMap receivedAcks;
   WaitAckNotification notifier;

   bool allAcksReceived = false;

   // note: we use uint for tv_sec (not uint64) because 32 bits are enough here
   std::string ackIDPrefix =
      StringTk::uintToHexStr(TimeAbs().getTimeval()->tv_sec) + "-" +
      StringTk::uintToHexStr(incAckCounter() ) + "-"
      "rlck" "-";


   if (notifyList.empty())
      return; // nothing to be done


   // create and register waitAcks

   /* note: waitAcks store pointers to notifyList items, so make sure to not remove anything from
      the list while we're still using the waitAcks pointers */

   for (LockRangeNotifyListIter iter = notifyList.begin(); iter != notifyList.end(); iter++)
   {
      std::string ackID = ackIDPrefix + iter->lockAckID; // (we assume lockAckID is globally unique)

      WaitAck waitAck(ackID, &(*iter) );

      waitAcks.insert(WaitAckMapVal(ackID, waitAck) );
   }

   ackStore->registerWaitAcks(&waitAcks, &receivedAcks, &notifier);


   // loop: send requests -> waitforcompletion -> resend

   while(numRetriesLeft && !app->getSelfTerminate() )
   {
      // create waitAcks copy

      WaitAckMap currentWaitAcks;
      {
         const std::lock_guard<Mutex> lock(notifier.waitAcksMutex);

         currentWaitAcks = waitAcks;
      }

      // send messages

      for(WaitAckMapIter iter = currentWaitAcks.begin(); iter != currentWaitAcks.end(); iter++)
      {
         RangeLockDetails* lockDetails = (RangeLockDetails*)iter->second.privateData;

         LockGrantedMsg msg(lockDetails->lockAckID, iter->first, localNodeID);

         std::pair<bool, unsigned> serializeRes = msg.serializeMessage(bufOut, bufOutLen);
         if(unlikely(!serializeRes.first) )
         { // buffer too small - should never happen
            logger->log(Log_CRITICAL, logContext, "BUG(?): Buffer too small for message "
               "serialization: " + StringTk::intToStr(bufOutLen) + "/" +
               StringTk::intToStr(serializeRes.second) );
            continue;
         }

         auto node = clients->referenceNode(lockDetails->clientNumID);
         if(unlikely(!node) )
         { // node not exists
            logger->log(Log_DEBUG, logContext, "Cannot grant lock to unknown client: " +
               lockDetails->clientNumID.str());
            continue;
         }

         dgramLis->sendBufToNode(*node, bufOut, serializeRes.second);
      }

      // wait for acks

      allAcksReceived = ackStore->waitForAckCompletion(&currentWaitAcks, &notifier, ackWaitSleepMS);
      if(allAcksReceived)
         break; // all acks received

      // some waitAcks left => prepare next loop

      numRetriesLeft--;
   }

   // waiting for acks is over

   ackStore->unregisterWaitAcks(&waitAcks);

   // check and handle results (waitAcks now contains all unreceived acks)

   if (waitAcks.empty())
   {
      LOG_DBG(GENERAL, DEBUG, "Stats: received all acks.", receivedAcks.size(), notifyList.size());
      return; // perfect, all acks received
   }

   // some acks were missing...

   logger->log(Log_DEBUG, logContext, "Some replies to lock grants missing. Received: " +
      StringTk::intToStr(receivedAcks.size() ) + "/" +
      StringTk::intToStr(receivedAcks.size() + waitAcks.size() ) );

   // the inode is supposed to be be referenced already
   MetaFileHandle inode = metaStore->referenceLoadedFile(this->parentEntryID, this->isBuddyMirrored,
      this->entryID);
   if(unlikely(!inode) )
   { // locked inode cannot be referenced
      logger->log(Log_DEBUG, logContext, "FileID cannot be referenced (file unlinked?): "
         + this->entryID);
      return;
   }

   // unlock all locks for which we didn't receive an ack

   for(WaitAckMapIter iter = waitAcks.begin(); iter != waitAcks.end(); iter++)
   {
      RangeLockDetails* lockDetails = (RangeLockDetails*)iter->second.privateData;
      lockDetails->setUnlock();

      inode->flockRange(*lockDetails);

      LOG_DEBUG(logContext, Log_DEBUG, "Reply was missing from: " + lockDetails->clientNumID.str());
   }

   // cancel all remaining lock waiters if too many acks were missing
   // (this is very important to avoid long timeouts it multiple clients are gone/disconnected)

   if(waitAcks.size() > 1)
   { // cancel all waiters
      inode->flockRangeCancelAllWaiters();
   }

   // cleanup

   metaStore->releaseFile(this->parentEntryID, inode);
}

unsigned LockRangeNotificationWork::incAckCounter()
{
   const std::lock_guard<Mutex> lock(ackCounterMutex);

   return ackCounter++;
}

Mutex* LockRangeNotificationWork::getDGramLisMutex(AbstractDatagramListener* dgramLis)
{
   return dgramLis->getSendMutex();
}
