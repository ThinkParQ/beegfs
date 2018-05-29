#ifndef BUDDYRESYNCER_H_
#define BUDDYRESYNCER_H_

#include <components/buddyresyncer/BuddyResyncJob.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>

#include <mutex>

/**
 * This component does not represent a thread by itself. Instead, it manages a group of "slave
 * threads" that are started and stopped when needed.
 *
 * Other components should only use this component as an interface and not access the slave threads
 * directly.
 */
class BuddyResyncer
{
   public:
      BuddyResyncer()
         : job(NULL), noNewResyncs(false)
      { }

      ~BuddyResyncer();

      FhgfsOpsErr startResync();
      void shutdown();

      static void commitThreadChangeSet();

   private:
      BuddyResyncJob* job; // Note: In the Storage Server, this is a Map. Here it's just one pointer
                           // that's set to NULL when no job is present.
      Mutex jobMutex;

   public:
      BuddyResyncJob* getResyncJob()
      {
         std::lock_guard<Mutex> lock(jobMutex);
         return job;
      }

      static void registerSyncChangeset()
      {
         BEEGFS_BUG_ON(currentThreadChangeSet, "current changeset not nullptr");

         currentThreadChangeSet = new MetaSyncCandidateFile;
      }

      static void abandonSyncChangeset()
      {
         delete currentThreadChangeSet;
         currentThreadChangeSet = nullptr;
      }

      static MetaSyncCandidateFile* getSyncChangeset()
      {
         return currentThreadChangeSet;
      }

   private:
      static __thread MetaSyncCandidateFile* currentThreadChangeSet;

      bool noNewResyncs;

      // No copy allowed
      BuddyResyncer(const BuddyResyncer&);
      BuddyResyncer& operator=(const BuddyResyncer&);
};

#endif /* BUDDYRESYNCER_H_ */
