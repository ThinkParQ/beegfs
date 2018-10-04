#include "ModificationEventHandler.h"

#include <common/toolkit/ZipIterator.h>
#include <database/FsckDBException.h>

#include <program/Program.h>

#include <mutex>

ModificationEventHandler::ModificationEventHandler(FsckDBModificationEventsTable& table)
   : PThread("ModificationEventHandler"),
     table(&table)
{
}

void ModificationEventHandler::run()
{
   FsckDBModificationEventsTable::BulkHandle bulkHandle(table->newBulkHandle() );
   while ( !getSelfTerminate() )
   {
      std::unique_lock<Mutex> bufferListSafeLock(bufferListMutex); // LOCK BUFFER

      // make sure to group at least MODHANDLER_MINSIZE_FLUSH flush elements (to not bother the DB
      // with every single event)
      if (bufferList.size() < MODHANDLER_MINSIZE_FLUSH)
      {
         bufferListSafeLock.unlock(); // UNLOCK BUFFER
         const std::lock_guard<Mutex> lock(eventsAddedMutex);
         eventsAddedCond.timedwait(&eventsAddedMutex, 2000);
         continue;
      }
      else
      {
         // create a copy of the buffer list and flush this to DB, so that the buffer will become
         // free immediately and the incoming messages do not have to wait for DB
         FsckModificationEventList bufferListCopy;

         bufferListCopy.splice(bufferListCopy.begin(), bufferList);

         bufferListSafeLock.unlock(); // UNLOCK BUFFER

         table->insert(bufferListCopy, bulkHandle);
      }
   }

   // a last flush after component stopped
   FsckModificationEventList bufferListCopy;

   {
      const std::lock_guard<Mutex> bufferListLock(bufferListMutex);
      bufferListCopy.splice(bufferListCopy.begin(), bufferList);
   }

   table->insert(bufferListCopy, bulkHandle);
}

bool ModificationEventHandler::add(UInt8List& eventTypeList, StringList& entryIDList)
{
   const char* logContext = "ModificationEventHandler (add)";

   if ( unlikely(eventTypeList.size() != entryIDList.size()) )
   {
      LogContext(logContext).logErr("Unable to add events. The lists do not have equal sizes.");
      return false;
   }

   while ( true )
   {
      {
         const std::lock_guard<Mutex> lock(bufferListMutex);
         if (this->bufferList.size() < MODHANDLER_MAXSIZE_EVENTLIST)
         {
            break;
         }
      }
      {
         const std::lock_guard<Mutex> lock(eventsFlushedMutex);
         this->eventsFlushedCond.timedwait(&eventsFlushedMutex, 2000);
      }
   }

   ZipIterRange<UInt8List, StringList> eventTypeEntryIDIter(eventTypeList, entryIDList);

   {
      const std::lock_guard<Mutex> bufferListLock(bufferListMutex);

      for ( ; !eventTypeEntryIDIter.empty(); ++eventTypeEntryIDIter)
      {
         FsckModificationEvent event((ModificationEventType)*(eventTypeEntryIDIter()->first),
            *(eventTypeEntryIDIter()->second) );
         this->bufferList.push_back(event);
      }
   }

   this->eventsAddedCond.signal();

   return true;
}
