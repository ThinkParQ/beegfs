#ifndef BUDDYRESYNCJOBSTATISTICS_H_
#define BUDDYRESYNCJOBSTATISTICS_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

enum BuddyResyncJobState
{
   BuddyResyncJobState_NOTSTARTED = 0,
   BuddyResyncJobState_RUNNING,
   BuddyResyncJobState_SUCCESS,
   BuddyResyncJobState_INTERRUPTED,
   BuddyResyncJobState_FAILURE,
   BuddyResyncJobState_ERRORS
};

class BuddyResyncJobStatistics
{
   public:
      BuddyResyncJobStatistics(BuddyResyncJobState state, int64_t startTime, int64_t endTime) :
         state(state),
         startTime(startTime),
         endTime(endTime)
      { }

      BuddyResyncJobStatistics() :
         state(BuddyResyncJobState_NOTSTARTED),
         startTime(0),
         endTime(0)
      { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::as<int32_t>(obj->state)
            % obj->startTime
            % obj->endTime;
      }

   private:
      BuddyResyncJobState state;
      int64_t startTime;
      int64_t endTime;

   public:
      BuddyResyncJobState getState() const { return state; }
      int64_t getStartTime() const { return startTime; }
      int64_t getEndTime() const { return endTime; }
};

class StorageBuddyResyncJobStatistics : public BuddyResyncJobStatistics
{
   public:
      StorageBuddyResyncJobStatistics(BuddyResyncJobState state, int64_t startTime, int64_t endTime,
            uint64_t discoveredFiles, uint64_t discoveredDirs, uint64_t matchedFiles,
            uint64_t matchedDirs, uint64_t syncedFiles, uint64_t syncedDirs, uint64_t errorFiles = 0,
            uint64_t errorDirs = 0) :
         BuddyResyncJobStatistics(state, startTime, endTime),
         discoveredFiles(discoveredFiles),
         discoveredDirs(discoveredDirs),
         matchedFiles(matchedFiles),
         matchedDirs(matchedDirs),
         syncedFiles(syncedFiles),
         syncedDirs(syncedDirs),
         errorFiles(errorFiles),
         errorDirs(errorDirs)
      {}

      StorageBuddyResyncJobStatistics() :
         BuddyResyncJobStatistics(),
         discoveredFiles(0),
         discoveredDirs(0),
         matchedFiles(0),
         matchedDirs(0),
         syncedFiles(0),
         syncedDirs(0),
         errorFiles(0),
         errorDirs(0)
      {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::base<BuddyResyncJobStatistics>(obj)
            % obj->discoveredFiles
            % obj->discoveredDirs
            % obj->matchedFiles
            % obj->matchedDirs
            % obj->syncedFiles
            % obj->syncedDirs
            % obj->errorFiles
            % obj->errorDirs;
      }

   private:
      uint64_t discoveredFiles;
      uint64_t discoveredDirs;
      uint64_t matchedFiles;
      uint64_t matchedDirs;
      uint64_t syncedFiles;
      uint64_t syncedDirs;
      uint64_t errorFiles;
      uint64_t errorDirs;

   public:
      uint64_t getDiscoveredDirs() const { return discoveredDirs; }
      uint64_t getDiscoveredFiles() const { return discoveredFiles; }
      uint64_t getMatchedDirs() const { return matchedDirs; }
      uint64_t getMatchedFiles() const { return matchedFiles; }
      uint64_t getSyncedDirs() const { return syncedDirs; }
      uint64_t getSyncedFiles() const { return syncedFiles; }
      uint64_t getErrorDirs() const { return errorDirs; }
      uint64_t getErrorFiles() const { return errorFiles; }
};

class MetaBuddyResyncJobStatistics : public BuddyResyncJobStatistics
{
   public:
      MetaBuddyResyncJobStatistics(BuddyResyncJobState state, int64_t startTime, int64_t endTime,
            uint64_t dirsDiscovered, uint64_t gatherErrors,
            uint64_t dirsSynced, uint64_t filesSynced, uint64_t dirErrors, uint64_t fileErrors,
            uint64_t sessionsToSync, uint64_t sessionsSynced, bool sessionSyncErrors,
            uint64_t modObjectsSynced, uint64_t modSyncErrors) :
         BuddyResyncJobStatistics(state, startTime, endTime),
         dirsDiscovered(dirsDiscovered),
         gatherErrors(gatherErrors),
         dirsSynced(dirsSynced),
         filesSynced(filesSynced),
         dirErrors(dirErrors),
         fileErrors(fileErrors),
         sessionsToSync(sessionsToSync),
         sessionsSynced(sessionsSynced),
         sessionSyncErrors(sessionSyncErrors),
         modObjectsSynced(modObjectsSynced),
         modSyncErrors(modSyncErrors)
      {}

      MetaBuddyResyncJobStatistics() :
         BuddyResyncJobStatistics(),
         dirsDiscovered(0),
         gatherErrors(0),
         dirsSynced(0),
         filesSynced(0),
         dirErrors(0),
         fileErrors(0),
         sessionsToSync(0),
         sessionsSynced(0),
         sessionSyncErrors(false),
         modObjectsSynced(0),
         modSyncErrors(0)
      {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::base<BuddyResyncJobStatistics>(obj)
            % obj->dirsDiscovered
            % obj->gatherErrors
            % obj->dirsSynced
            % obj->filesSynced
            % obj->dirErrors
            % obj->fileErrors
            % obj->sessionsToSync
            % obj->sessionsSynced
            % obj->sessionSyncErrors
            % obj->modObjectsSynced
            % obj->modSyncErrors;
      }

   private:
      uint64_t dirsDiscovered;
      uint64_t gatherErrors;

      uint64_t dirsSynced;
      uint64_t filesSynced;
      uint64_t dirErrors;
      uint64_t fileErrors;

      uint64_t sessionsToSync;
      uint64_t sessionsSynced;
      bool sessionSyncErrors;

      uint64_t modObjectsSynced;
      uint64_t modSyncErrors;

   public:
      uint64_t getDirsDiscovered() { return dirsDiscovered; }
      uint64_t getGatherErrors() { return gatherErrors; }
      uint64_t getDirsSynced() { return dirsSynced; }
      uint64_t getFilesSynced() { return filesSynced; }
      uint64_t getDirErrors() { return dirErrors; }
      uint64_t getFileErrors() { return fileErrors; }
      uint64_t getSessionsToSync() { return sessionsToSync; }
      uint64_t getSessionsSynced() { return sessionsSynced; }
      bool getSessionSyncErrors() { return sessionSyncErrors; }
      uint64_t getModObjectsSynced() { return modObjectsSynced; }
      uint64_t getModSyncErrors() { return modSyncErrors; }
};

#endif /* BUDDYRESYNCJOBSTATS_H_ */
