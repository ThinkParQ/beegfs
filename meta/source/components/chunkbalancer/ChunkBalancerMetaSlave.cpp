#include <app/App.h>
#include <storage/MetaStore.h>
#include <program/Program.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <components/FileEventLogger.h>

#include "ChunkBalancerMetaSlave.h"

ChunkBalancerMetaSlave::ChunkBalancerMetaSlave(ChunkBalancerJob& parentJob, ChunkSyncCandidateStore* copyCandidates, uint8_t  slaveID) :
   SyncSlaveBase("ChunkBalancerMetaSlave_" + std::to_string(slaveID), parentJob),
      copyCandidates(copyCandidates)
{}

ChunkBalancerMetaSlave::~ChunkBalancerMetaSlave()
{
}

void ChunkBalancerMetaSlave::syncLoop()
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   GlobalInodeLockStore* inodeLockStore = metaStore->getInodeLockStore();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();

   const char* logContext = "ChunkBalanceMetaSlave running";
   while (!getSelfTerminateNotIdle())
   {
      bool mirroredChunk = false;
      ChunkSyncCandidateFile candidate;
      FhgfsOpsErr workRes;
      SynchronizedCounter counter;

      if ((copyCandidates->isFilesEmpty()))
         continue;

      copyCandidates->fetch(candidate, this);  //fetch information on chunk to be copied
      if (unlikely(candidate.getTargetID() == 0)) // ignore targetID 0
         continue;

      IdType idType = candidate.getIdType();
      std::string relativePath = candidate.getRelativePath();
      uint16_t localTargetID = candidate.getTargetID();
      uint16_t destinationID = candidate.getDestinationID();
      EntryInfo* entryInfo = candidate.getEntryInfo();
      FileEvent* fileEvent = candidate.getFileEvent();

      if (idType == idType_GROUP) //case of mirrored chunk and IDs given are group IDs
      {
         mirroredChunk = true;
      }

      DirInode* subDir = metaStore->referenceDir(entryInfo->getParentEntryID(),
         entryInfo->getIsBuddyMirrored(), false);
      if (!subDir)
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to access parent directory, chunk will not be balanced. chunkPath: " + relativePath + "; localTargetID: "
         + std::to_string(localTargetID) + "; destinationTargetID: "
         + std::to_string(destinationID));
         errorCount.increase();
         continue;
      }
      { //Metastore lock scope, will release lock on Metastore automatically when scope ends
         UniqueRWLock lock(metaStore->rwlock, SafeRWLock_WRITE); // Write LOCK Metastore until we add inode to GlobalInodeLockStore

         FhgfsOpsErr unlinkableRes = metaStore->isFileUnlinkable(*subDir, entryInfo);
         if (unlinkableRes)
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to access file inode lock, chunk will not be balanced. chunkPath: " + relativePath + "; localTargetID: "
            + std::to_string(localTargetID) + "; destinationTargetID: "
            + std::to_string(destinationID));
            errorCount.increase();
            metaStore->releaseDirUnlocked(entryInfo->getParentEntryID());
            continue;
         }
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_DEBUG, "Lock inode operation started. chunkPath: " + relativePath + "; entryID: "
            + entryInfo->getEntryID() + "; localTargetID: "
            + std::to_string(localTargetID) + "; destinationTargetID: "
            + std::to_string(destinationID)+ "; Idtype: "
            + std::to_string(idType));

         FhgfsOpsErr lockRes = inodeLockStore->insertFileInode(entryInfo, fileEvent, true,
                                                               LockOperationType::CHUNK_REBALANCING);
         if (lockRes != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
               "Unable to lock file inode, chunk will not be balanced. "
               "chunkPath: " + relativePath + "; localTargetID: " +
               std::to_string(localTargetID) + "; destinationTargetID: " +
               std::to_string(destinationID));
            errorCount.increase();
            metaStore->releaseDirUnlocked(entryInfo->getParentEntryID());
            continue;
         }

         metaStore->releaseDirUnlocked(entryInfo->getParentEntryID());
      } //end of Metastore lock scope, releases lock on Metastore

      //check if destinationID is already in stripe pattern
      FileInode* inode = inodeLockStore->getFileInodeUnreferenced(entryInfo);
      if (!inode)
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Unable to access inode, chunk will not be balanced. chunkPath: " + relativePath + "; localTargetID: "
         + std::to_string(localTargetID) + "; destinationTargetID: "
         + std::to_string(destinationID));
         errorCount.increase();
         continue;
      }

      StripePattern* pattern = inode->getStripePattern();
      const UInt16Vector* stripeTargetIDs = pattern->getStripeTargetIDs();

      //check if sourceID is in stripe pattern
      if (unlikely((std::find(stripeTargetIDs->begin(), stripeTargetIDs->end(), localTargetID) == stripeTargetIDs->end())) )
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "sourceID given is not in stripe pattern, wrong ID given or chunk was already balanced: " + relativePath + "; localTargetID: "
            + std::to_string(localTargetID) + "; destinationTargetID: "
            + std::to_string(destinationID));
            inodeLockStore->releaseFileInode(entryInfo->getEntryID(), LockOperationType::CHUNK_REBALANCING); //release access to the file inode on primary
            errorCount.increase();
            continue;
      }
      //check if destinationID is in stripe pattern
      else if (unlikely((std::find(stripeTargetIDs->begin(), stripeTargetIDs->end(), destinationID) != stripeTargetIDs->end())) )
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "DestinationID given is already in stripe pattern, chunk will not be balanced: " + relativePath + "; localTargetID: "
            + std::to_string(localTargetID) + "; destinationTargetID: "
            + std::to_string(destinationID));
            inodeLockStore->releaseFileInode(entryInfo->getEntryID(), LockOperationType::CHUNK_REBALANCING); //release access to the file inode on primary
            errorCount.increase();
            continue;
      }

      CopyChunkFileWork* work = new CopyChunkFileWork(localTargetID, destinationID, relativePath, entryInfo, mirroredChunk,  &workRes, &counter, fileEvent);

      slaveQ->addDirectWork(work);

      //wait for work completion
      counter.waitForCount(1);
      if (workRes == FhgfsOpsErr_AGAIN) //CB queue on storage is full, log error message
      {
         LogContext(logContext).log(Log_WARNING,
            "ChunkBalanceQueue on storage is full, chunk will not be balanced. Consider increasing queue size and resubmit job. "
            "TargetID:  " + std::to_string(localTargetID)  + " ; "
            "Relative path: " + relativePath);
            inodeLockStore->releaseFileInode(entryInfo->getEntryID(), LockOperationType::CHUNK_REBALANCING); //release access to the file inode on primary
            errorCount.increase();
      }
      else if (unlikely(workRes!= FhgfsOpsErr_SUCCESS))
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during start of copy chunk operation. "
            "TargetID:  " + std::to_string(localTargetID)  + " ; "
            "Relative path: " + relativePath);
         inodeLockStore->releaseFileInode(entryInfo->getEntryID(), LockOperationType::CHUNK_REBALANCING); //release access to the file inode on primary
         errorCount.increase();
      }
      numChunksSynced.increase();
   }  
}











