#ifndef META_SESSIONFILESTORERESYNCER_H
#define META_SESSIONFILESTORERESYNCER_H

#include <common/nodes/Node.h>
#include <common/threading/PThread.h>

class SessionStoreResyncer
{
   friend class BuddyResyncer;
   friend class BuddyResyncJob;

   public:
      SessionStoreResyncer(const NumNodeID& buddyNodeID);

      struct Stats
      {
         uint64_t sessionsToSync;
         uint64_t sessionsSynced;
         bool errors;
      };

      Stats getStats()
      {
         return Stats{ numSessionsToSync.read(), numSessionsSynced.read(), errors.read() != 0 };
      }

   private:
      NumNodeID buddyNodeID;

      AtomicUInt64 numSessionsToSync;
      AtomicUInt64 numSessionsSynced;
      AtomicSizeT errors; // 0 / 1

      void doSync();
};

#endif /* META_SESSIONFILESTORERESYNCER_H */
