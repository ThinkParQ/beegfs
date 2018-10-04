#ifndef META_BUDDYRESYNCERMODSYNCSLAVE_H
#define META_BUDDYRESYNCERMODSYNCSLAVE_H

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/threading/PThread.h>
#include <common/storage/StorageErrors.h>
#include <common/nodes/Node.h>
#include <app/App.h>
#include <components/buddyresyncer/SyncCandidate.h>

#include "SyncSlaveBase.h"

class DirEntry;

class BuddyResyncerModSyncSlave : public SyncSlaveBase
{
   friend class BuddyResyncer;
   friend class BuddyResyncJob;

   public:
      BuddyResyncerModSyncSlave(BuddyResyncJob& parentJob, MetaSyncCandidateStore* syncCandidates,
            uint8_t slaveID, const NumNodeID& buddyNodeID);

      struct Stats
      {
         uint64_t objectsSynced;
         uint64_t errors;
      };

      Stats getStats()
      {
         return Stats{ numObjectsSynced.read(), numErrors.read() };
      }

   private:
      MetaSyncCandidateStore* syncCandidates;

      AtomicUInt64 numObjectsSynced;
      AtomicUInt64 numErrors;

      void syncLoop();

      FhgfsOpsErr streamCandidates(Socket& socket);

   private:
      static FhgfsOpsErr streamCandidates(Socket* socket, void* context)
      {
         return static_cast<BuddyResyncerModSyncSlave*>(context)->streamCandidates(*socket);
      }
};

#endif
