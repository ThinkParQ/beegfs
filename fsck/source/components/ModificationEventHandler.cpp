#include "ModificationEventHandler.h"

#include <common/toolkit/ZipIterator.h>
#include <database/FsckDBException.h>

#include <program/Program.h>

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
      SafeMutexLock bufferListSafeLock(&bufferListMutex); // LOCK BUFFER

      // make sure to group at least MODHANDLER_MINSIZE_FLUSH flush elements (to not bother the DB
      // with every single event)
      if (bufferList.size() < MODHANDLER_MINSIZE_FLUSH)
      {
         bufferListSafeLock.unlock(); // UNLOCK BUFFER
         SafeMutexLock lock(&eventsAddedMutex);
         eventsAddedCond.timedwait(&eventsAddedMutex, 2000);
         lock.unlock();
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

   SafeMutexLock bufferListSafeLock(&bufferListMutex); // LOCK BUFFER
   bufferListCopy.splice(bufferListCopy.begin(), bufferList);
   bufferListSafeLock.unlock(); // UNLOCK BUFFER

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
      SafeMutexLock bufferListSafeLock(&bufferListMutex);
      if (this->bufferList.size() < MODHANDLER_MAXSIZE_EVENTLIST)
      {
         bufferListSafeLock.unlock();
         break;
      }
      else
      {
         bufferListSafeLock.unlock();
         SafeMutexLock eventsFlushedSafeLock(&eventsFlushedMutex);
         this->eventsFlushedCond.timedwait(&eventsFlushedMutex, 2000);
         eventsFlushedSafeLock.unlock();
      }
   }

   ZipIterRange<UInt8List, StringList> eventTypeEntryIDIter(eventTypeList, entryIDList);

   SafeMutexLock bufferListSafeLock(&bufferListMutex);

   for ( ; !eventTypeEntryIDIter.empty(); ++eventTypeEntryIDIter)
   {
      FsckModificationEvent event((ModificationEventType)*(eventTypeEntryIDIter()->first),
         *(eventTypeEntryIDIter()->second) );
      this->bufferList.push_back(event);
   }

   bufferListSafeLock.unlock();

   this->eventsAddedCond.signal();

   return true;
}
