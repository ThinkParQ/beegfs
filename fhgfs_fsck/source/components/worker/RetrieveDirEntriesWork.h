#ifndef RETRIEVEDIRENTRIESWORK_H
#define RETRIEVEDIRENTRIESWORK_H

/*
 * retrieve all dir entries from one node, inside a specified range of hashDirs and save them to DB
 */

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>

#include <database/FsckDB.h>
#include <database/FsckDBTable.h>

// the size of one response packet, i.e. how many dentries are asked for at once
#define RETRIEVE_DIR_ENTRIES_PACKET_SIZE 500

class RetrieveDirEntriesWork : public Work
{
   public:
      /*
       * @param db database instance
       * @param node the node to retrieve data from
       * @param counter a pointer to a Synchronized counter; this is incremented by one at the end
       * and the calling thread can wait for the counter
       * @param hashDirStart the first top-level hashDir to open
       * @param hashDirEnd the last top-level hashDir to open
       * @param numDentriesFound
       * @param numFileInodesFound
       */
      RetrieveDirEntriesWork(FsckDB* db, Node& node, SynchronizedCounter* counter,
         AtomicUInt64& errors, unsigned hashDirStart, unsigned hashDirEnd,
         AtomicUInt64* numDentriesFound, AtomicUInt64* numFileInodesFound,
         std::set<FsckTargetID>& usedTargets);

      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

   private:
      LogContext log;
      Node& node;
      SynchronizedCounter* counter;
      AtomicUInt64* errors;
      AtomicUInt64* numDentriesFound;
      AtomicUInt64* numFileInodesFound;
      std::set<FsckTargetID>* usedTargets;

      unsigned hashDirStart;
      unsigned hashDirEnd;

      FsckDBDentryTable* dentries;
      FsckDBDentryTable::BulkHandle dentriesHandle;

      FsckDBFileInodesTable* files;
      FsckDBFileInodesTable::BulkHandle filesHandle;

      FsckDBContDirsTable* contDirs;
      FsckDBContDirsTable::BulkHandle contDirsHandle;

      void doWork(bool isBuddyMirrored);
};

#endif /* RETRIEVEDIRENTRIESWORK_H */
