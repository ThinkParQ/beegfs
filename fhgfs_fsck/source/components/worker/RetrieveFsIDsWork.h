#ifndef RETRIEVEFSIDSWORK_H
#define RETRIEVEFSIDSWORK_H

/*
 * retrieve all FS-IDs from one node, inside a specified range of hashDirs and save them to DB
 */

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>

#include <database/FsckDB.h>
#include <database/FsckDBTable.h>

// the size of one response packet, i.e. how many fsids are asked for at once
#define RETRIEVE_FSIDS_PACKET_SIZE 1000

class RetrieveFsIDsWork : public Work
{
   public:
      /*
       * @param db database instance
       * @param node the node to retrieve data from
       * @param counter a pointer to a Synchronized counter; this is incremented by one at the end
       * and the calling thread can wait for the counter
       * @param hashDirStart the first top-level hashDir to open
       * @param hashDirEnd the last top-level hashDir to open
       */
      RetrieveFsIDsWork(FsckDB* db, Node& node, SynchronizedCounter* counter, AtomicUInt64& errors,
         unsigned hashDirStart, unsigned hashDirEnd);
      virtual ~RetrieveFsIDsWork();
      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

   private:
      LogContext log;
      Node& node;
      SynchronizedCounter* counter;
      AtomicUInt64* errors;

      unsigned hashDirStart;
      unsigned hashDirEnd;

      FsckDBFsIDsTable* table;
      FsckDBFsIDsTable::BulkHandle bulkHandle;

      void doWork(bool isBuddyMirrored);
};

#endif /* RETRIEVEFSIDSWORK_H */
