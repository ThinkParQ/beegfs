#ifndef RETRIEVEINODESWORK_H
#define RETRIEVEINODESWORK_H

/*
 * retrieve all inodes from one node, inside a specified range of hashDirs and save them to DB

 */

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <database/FsckDB.h>
#include <database/FsckDBTable.h>

// the size of one response packet, i.e. how many inodes are asked for at once
#define RETRIEVE_INODES_PACKET_SIZE 500

class RetrieveInodesWork : public Work
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
      RetrieveInodesWork(FsckDB* db, Node& node, SynchronizedCounter* counter,
         AtomicUInt64& errors, unsigned hashDirStart, unsigned hashDirEnd,
         AtomicUInt64* numFileInodesFound, AtomicUInt64* numDirInodesFound,
         std::set<FsckTargetID>& usedTargets);

      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

   private:
      LogContext log;
      Node& node;
      SynchronizedCounter* counter;
      AtomicUInt64* errors;
      std::set<FsckTargetID>* usedTargets;

      unsigned hashDirStart;
      unsigned hashDirEnd;

      AtomicUInt64* numFileInodesFound;
      AtomicUInt64* numDirInodesFound;

      FsckDBFileInodesTable* files;
      FsckDBFileInodesTable::BulkHandle filesHandle;

      FsckDBDirInodesTable* dirs;
      FsckDBDirInodesTable::BulkHandle dirsHandle;

      void doWork(bool isBuddyMirrored);
};

#endif /* RETRIEVEINODESWORK_H */
