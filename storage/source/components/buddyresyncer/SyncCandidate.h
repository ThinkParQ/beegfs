#ifndef STORAGE_COMPONENTS_BUDDYRESYNCER_SYNCCANDIDATE_H
#define STORAGE_COMPONENTS_BUDDYRESYNCER_SYNCCANDIDATE_H

#include <common/storage/mirroring/SyncCandidateStore.h>

#include <string>

/**
 * A storage sync candidate. Has a target ID and a path.
 */
class ChunkSyncCandidateDir
{
   public:
      ChunkSyncCandidateDir(const std::string& relativePath, const uint16_t targetID)
         : relativePath(relativePath), targetID(targetID)
      { }

      ChunkSyncCandidateDir()
         : targetID(0)
      { }

   private:
      std::string relativePath;
      uint16_t targetID;

   public:
      const std::string& getRelativePath() const { return relativePath; }
      uint16_t getTargetID() const               { return targetID; }
};

/**
 * A storage sync candidate that also has an onlyAttribs flag.
 */
class ChunkSyncCandidateFile : public ChunkSyncCandidateDir
{
   public:
      ChunkSyncCandidateFile(const std::string& relativePath, uint16_t targetID)
         : ChunkSyncCandidateDir(relativePath, targetID)
      { }

      ChunkSyncCandidateFile() = default;
};

typedef SyncCandidateStore<ChunkSyncCandidateDir, ChunkSyncCandidateFile> ChunkSyncCandidateStore;

#endif /* STORAGE_COMPONENTS_BUDDYRESYNCER_SYNCCANDIDATE_H */
