#ifndef DATAFETCHER_H_
#define DATAFETCHER_H_

#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/nodes/Node.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <database/FsckDB.h>

#define DATAFETCHER_OUTPUT_INTERVAL_MS 2000

class DataFetcher
{
   private:
      FsckDB* database;

      MultiWorkQueue* workQueue;

      AtomicUInt64 numDentriesFound;
      AtomicUInt64 numFileInodesFound;
      AtomicUInt64 numDirInodesFound;
      AtomicUInt64 numChunksFound;

   public:
      DataFetcher(FsckDB& db, bool forceRestart);

      FhgfsOpsErr execute();

   private:
      SynchronizedCounter finishedPackages;
      AtomicUInt64 fatalErrorsFound;
      unsigned generatedPackages;
      bool forceRestart;

      std::list<std::set<FsckTargetID> > usedTargets;

      void retrieveDirEntries(const std::vector<NodeHandle>& nodes);
      void retrieveInodes(const std::vector<NodeHandle>& nodes);
      bool retrieveChunks();

      void printStatus(bool toLogFile = false);
};

#endif /* DATAFETCHER_H_ */
