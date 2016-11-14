#ifndef MODIFICATIONEVENTHANDLER_H
#define MODIFICATIONEVENTHANDLER_H

#include <common/fsck/FsckModificationEvent.h>
#include <common/threading/Condition.h>
#include <database/FsckDBTable.h>


#define MODHANDLER_MAXSIZE_EVENTLIST      50000
#define MODHANDLER_MINSIZE_FLUSH          200

class ModificationEventHandler: public PThread
{
   public:
      ModificationEventHandler(FsckDBModificationEventsTable& table);

      virtual void run();

      bool add(UInt8List& eventTypeList, StringList& entryIDList);

   private:
      FsckDBModificationEventsTable* table;

      FsckModificationEventList bufferList;

      Mutex bufferListMutex;
      Mutex bufferListCopyMutex;
      Mutex flushMutex;

      Mutex eventsAddedMutex;
      Condition eventsAddedCond;
      Mutex eventsFlushedMutex;
      Condition eventsFlushedCond;

   public:
      void stop()
      {
         selfTerminate();
         eventsAddedCond.signal();
         join();
      }
};

#endif /* MODIFICATIONEVENTHANDLER_H */
