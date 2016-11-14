#ifndef ADJUSTCHUNKPERMISSIONSWORK_H
#define ADJUSTCHUNKPERMISSIONSWORK_H

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>

#include <database/FsckDB.h>

// the size of one packet, i.e. how many files are adjusted at once; basically just
// limited to have some control and give the user feedback
#define ADJUST_AT_ONCE 50

class AdjustChunkPermissionsWork : public Work
{
   public:
      AdjustChunkPermissionsWork(Node& node, SynchronizedCounter* counter, AtomicUInt64* fileCount,
         AtomicUInt64* errorCount);
      virtual ~AdjustChunkPermissionsWork();
      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

   private:
      LogContext log;
      Node& node;
      SynchronizedCounter* counter;
      AtomicUInt64* fileCount;
      AtomicUInt64* errorCount;

      void doWork(bool isBuddyMirrored);
};

#endif /* ADJUSTCHUNKPERMISSIONSWORK_H */
