#include "ModificationEventFlusher.h"

#include <common/toolkit/SynchronizedCounter.h>
#include <common/toolkit/ListTk.h>
#include <common/net/message/fsck/FsckModificationEventMsg.h>

#include <program/Program.h>

#include <mutex>

ModificationEventFlusher::ModificationEventFlusher()
 : PThread("ModificationEventFlusher"),
   log("ModificationEventFlusher"),
   dGramLis(Program::getApp()->getDatagramListener() ),
   workerList(Program::getApp()->getWorkers() ),
   fsckMissedEvent(false)
{
   NicAddressList nicList;
   this->fsckNode = std::make_shared<Node>(NODETYPE_Invalid, "fsck", NumNodeID(), 0, 0, nicList);
   NicListCapabilities localNicCaps;
   NicAddressList localNicList = Program::getApp()->getLocalNicList();
   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   this->fsckNode->getConnPool()->setLocalNicList(localNicList, localNicCaps);
}

void ModificationEventFlusher::run()
{
   try
   {
      registerSignalHandler();

      while ( !this->getSelfTerminate() )
      {
         while ( this->eventTypeBufferList.empty() )
         {
            {
               const std::lock_guard<Mutex> lock(eventsAddedMutex);
               this->eventsAddedCond.timedwait(&eventsAddedMutex, 2000);
            }

            if ( this->getSelfTerminate() )
               goto stop_component;
         }

         // buffer list not empty... go ahead and send it
         this->sendToFsck();
      }

stop_component:
      log.log(Log_DEBUG, "Component stopped.");
   }
   catch (std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

bool ModificationEventFlusher::add(ModificationEventType eventType, const std::string& entryID)
{
   while (true)
   {
      {
         const std::lock_guard<Mutex> lock(mutex);

         if (this->eventTypeBufferList.size() < MODFLUSHER_MAXSIZE_EVENTLIST)
            break;
      }

      // queue too long
      // wait if something is flushed
      {
         const std::lock_guard<Mutex> lock(eventsFlushedMutex);
         this->eventsFlushedCond.timedwait(&eventsFlushedMutex, 5000);
      }
   }

   {
      const std::lock_guard<Mutex> lock(mutex);
      this->eventTypeBufferList.push_back((uint8_t)eventType);
      this->entryIDBufferList.push_back(entryID);
   }

   {
      const std::lock_guard<Mutex> lock(eventsAddedMutex);
      this->eventsAddedCond.broadcast();
   }

   return true;
}

void ModificationEventFlusher::sendToFsck()
{
   if (!fsckNode)
   {
      log.logErr("Fsck modification events are present, but fsck node is not set.");
      this->fsckMissedEvent = true;

      // stop logging
      this->disableLoggingLocally(false);

      return;
   }

   // get the first MODFLUSHER_SEND_AT_ONCE entries from each list and send them to fsck

   // only have the mutex on the lists as long as we really need it
   UInt8List eventTypeListCopy;
   StringList entryIDListCopy;

   {
      const std::lock_guard<Mutex> lock(mutex);

      UInt8ListIter eventTypeStart = this->eventTypeBufferList.begin();
      UInt8ListIter eventTypeEnd = this->eventTypeBufferList.begin();
      ListTk::advance(eventTypeBufferList, eventTypeEnd, MODFLUSHER_SEND_AT_ONCE);

      StringListIter entryIDStart = this->entryIDBufferList.begin();
      StringListIter entryIDEnd = this->entryIDBufferList.begin();
      ListTk::advance(entryIDBufferList, entryIDEnd, MODFLUSHER_SEND_AT_ONCE);

      eventTypeListCopy.splice(eventTypeListCopy.begin(), this->eventTypeBufferList, eventTypeStart,
         eventTypeEnd);
      entryIDListCopy.splice(entryIDListCopy.begin(), this->entryIDBufferList, entryIDStart,
         entryIDEnd);
   }

   FsckModificationEventMsg fsckModificationEventMsg(&eventTypeListCopy, &entryIDListCopy,
      this->fsckMissedEvent);

   bool ackReceived = this->dGramLis->sendToNodeUDPwithAck(fsckNode, &fsckModificationEventMsg,
      MODFLUSHER_WAIT_FOR_ACK_MS, MODFLUSHER_WAIT_FOR_ACK_RETRIES);

   if (!ackReceived)
   {
      log.log(Log_CRITICAL,
         "Did not receive an ack from fsck for a FsckModificationEventMsg");
      this->fsckMissedEvent = true;

      // stop logging
      this->disableLoggingLocally(false);
   }

   const std::lock_guard<Mutex> lock(eventsFlushedMutex);
   eventsFlushedCond.broadcast();
}
