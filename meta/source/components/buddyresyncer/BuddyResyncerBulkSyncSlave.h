#ifndef META_BUDDYRESYNCERBULKSYNCSLAVE_H
#define META_BUDDYRESYNCERBULKSYNCSLAVE_H

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/threading/PThread.h>
#include <common/storage/StorageErrors.h>
#include <common/nodes/Node.h>
#include <app/App.h>
#include <components/buddyresyncer/SyncCandidate.h>

#include "SyncSlaveBase.h"

class DirEntry;

class BuddyResyncerBulkSyncSlave : public SyncSlaveBase
{
   friend class BuddyResyncer;
   friend class BuddyResyncJob;

   public:
      BuddyResyncerBulkSyncSlave(BuddyResyncJob& parentJob, MetaSyncCandidateStore* syncCandidates, uint8_t slaveID,
         const NumNodeID& buddyNodeID);

      struct Stats
      {
         uint64_t dirsSynced;
         uint64_t filesSynced;
         uint64_t dirErrors;
         uint64_t fileErrors;
      };

      Stats getStats()
      {
         return Stats{ numDirsSynced.read(), numFilesSynced.read(),
            numDirErrors.read(), numFileErrors.read() };
      }

   private:
      MetaSyncCandidateStore* syncCandidates;

      AtomicUInt64 numDirsSynced;
      AtomicUInt64 numFilesSynced;
      AtomicUInt64 numDirErrors;
      AtomicUInt64 numFileErrors;

      void syncLoop();

      FhgfsOpsErr resyncDirectory(const MetaSyncCandidateDir& root, const std::string& inodeID);

      FhgfsOpsErr streamCandidateDir(Socket& socket, const MetaSyncCandidateDir& candidate,
         const std::string& inodeID);


   private:
      typedef std::tuple<
         BuddyResyncerBulkSyncSlave&,
         const MetaSyncCandidateDir&,
         const std::string&> StreamCandidateArgs;

      static FhgfsOpsErr streamCandidateDir(Socket* socket, void* context)
      {
         using std::get;

         auto& args = *(StreamCandidateArgs*) context;
         return get<0>(args).streamCandidateDir(*socket, get<1>(args), get<2>(args));
      }
};

#endif
