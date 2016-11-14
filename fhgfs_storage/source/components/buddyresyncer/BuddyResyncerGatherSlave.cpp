#include <app/App.h>
#include <toolkit/StorageTkEx.h>

#include <program/Program.h>

#include "BuddyResyncerGatherSlave.h"

Mutex BuddyResyncerGatherSlave::staticGatherSlavesMutex;
std::map<std::string, BuddyResyncerGatherSlave*> BuddyResyncerGatherSlave::staticGatherSlaves;

BuddyResyncerGatherSlave::BuddyResyncerGatherSlave(uint16_t targetID,
   ChunkSyncCandidateStore* syncCandidates, BuddyResyncerGatherSlaveWorkQueue* workQueue,
   uint8_t slaveID) :
   PThread("BuddyResyncerGatherSlave_" + StringTk::uintToStr(targetID) + "-" +
      StringTk::uintToStr(slaveID))
{
   this->isRunning = false;
   this->targetID = targetID;
   this->syncCandidates = syncCandidates;
   this->workQueue = workQueue;

   SafeMutexLock staticGatherSlavesLock(&staticGatherSlavesMutex);

   staticGatherSlaves[this->getName()] = this;

   staticGatherSlavesLock.unlock();
}

BuddyResyncerGatherSlave::~BuddyResyncerGatherSlave()
{
}


/**
 * This is a component, which is started through its control frontend on-demand at
 * runtime and terminates when it's done.
 * We have to ensure (in cooperation with the control frontend) that we don't get multiple instances
 * of this thread running at the same time.
 */
void BuddyResyncerGatherSlave::run()
{
   setIsRunning(true);

   numChunksDiscovered.setZero();
   numChunksMatched.setZero();
   numDirsDiscovered.setZero();
   numDirsMatched.setZero();

   try
   {
      LogContext(__func__).log(Log_DEBUG, "Component started.");

      registerSignalHandler();

      workLoop();

      LogContext(__func__).log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

   setIsRunning(false);
}

void BuddyResyncerGatherSlave::workLoop()
{
   const unsigned maxOpenFDsNum = 20; // max open FDs => max path sub-depth for efficient traversal

   while (!getSelfTerminateNotIdle())
   {
      if ((workQueue->queueEmpty()) && (getSelfTerminate()))
         break;

      // get a directory to scan
      std::string pathStr = workQueue->fetch(this);

      if(unlikely(pathStr.empty()))
         continue;

      int nftwRes = nftw(pathStr.c_str(), handleDiscoveredEntry, maxOpenFDsNum, FTW_ACTIONRETVAL);
      if(nftwRes == -1)
      { // error occurred
         LogContext(__func__).logErr("Error during chunks walk. SysErr: " + System::getErrString());
      }
   }
}

int BuddyResyncerGatherSlave::handleDiscoveredEntry(const char* path,
   const struct stat* statBuf, int ftwEntryType, struct FTW* ftwBuf)
{
   std::string targetPath;
   std::string chunksPath;

   SafeMutexLock staticGatherSlavesLock(&staticGatherSlavesMutex);

   BuddyResyncerGatherSlave* thisStatic = staticGatherSlaves[PThread::getCurrentThreadName()];

   staticGatherSlavesLock.unlock();

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   StorageTargets* storageTargets = app->getStorageTargets();

   storageTargets->getPath(thisStatic->getTargetID(), &targetPath);
   chunksPath = targetPath + "/" + CONFIG_BUDDYMIRROR_SUBDIR_NAME;

   if (strlen(path) <= chunksPath.length())
      return FTW_CONTINUE;

   std::string relPathStr = path + chunksPath.size() + 1;

   if ( relPathStr.empty() )
      return FTW_CONTINUE;

   bool buddyCommIsOverride = false; // silence warning
   int64_t lastBuddyCommTimeSecs =
      storageTargets->readLastBuddyCommTimestamp(thisStatic->getTargetID(), &buddyCommIsOverride);
   int64_t lastBuddyCommSafetyThresholdSecs = cfg->getSysResyncSafetyThresholdMins()*60;
   if ( (lastBuddyCommSafetyThresholdSecs == 0) && (!buddyCommIsOverride) ) // ignore timestamp file
      lastBuddyCommTimeSecs = 0;
   else
   if (lastBuddyCommTimeSecs > lastBuddyCommSafetyThresholdSecs)
      lastBuddyCommTimeSecs -= lastBuddyCommSafetyThresholdSecs; // TODO is this correct?

   if(ftwEntryType == FTW_D) // directory
   {
      thisStatic->numDirsDiscovered.increase();

      int64_t dirModificationTime = (int64_t)statBuf->st_mtim.tv_sec;

      if(dirModificationTime > lastBuddyCommTimeSecs)
      { // sync candidate
         ChunkSyncCandidateDir candidate(relPathStr, thisStatic->getTargetID());
         thisStatic->syncCandidates->add(candidate, thisStatic);
         thisStatic->numDirsMatched.increase();
      }
   }
   else
   if(ftwEntryType == FTW_F) // file
   {
      // we found a chunk
      thisStatic->numChunksDiscovered.increase();

      // we need to use ctime here, because mtime can be set manually (even to the future)
      time_t chunkChangeTime = statBuf->st_ctim.tv_sec;

      if(chunkChangeTime > lastBuddyCommTimeSecs)
      {  // sync candidate
         std::string relPathStr = path + chunksPath.size() + 1;

         ChunkSyncCandidateFile candidate(relPathStr, thisStatic->getTargetID());
         thisStatic->syncCandidates->add(candidate, thisStatic);

         thisStatic->numChunksMatched.increase();
      }
   }

   return FTW_CONTINUE;
}
