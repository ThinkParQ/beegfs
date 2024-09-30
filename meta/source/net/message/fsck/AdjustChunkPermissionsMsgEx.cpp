#include "AdjustChunkPermissionsMsgEx.h"

#include <program/Program.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <components/worker/SetChunkFileAttribsWork.h>

bool AdjustChunkPermissionsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Incoming AdjustChunkPermissionsMsg");

   MetaStore *metaStore = Program::getApp()->getMetaStore();

   unsigned hashDirNum = this->getHashDirNum();
   unsigned maxEntries = this->getMaxEntries();
   int64_t lastHashDirOffset = this->getLastHashDirOffset();
   int64_t lastContDirOffset = this->getLastContDirOffset();
   std::string currentContDirID = this->getCurrentContDirID();
   int64_t newHashDirOffset = 0;
   int64_t newContDirOffset = 0;
   unsigned errorCount = 0;

   unsigned readOutEntries = 0;

   bool hasNext;

   if ( currentContDirID.empty() )
   {
      hasNext = StorageTkEx::getNextContDirID(hashDirNum, getIsBuddyMirrored(), lastHashDirOffset,
         &currentContDirID, &newHashDirOffset);
      if ( hasNext )
      {
         lastHashDirOffset = newHashDirOffset;
      }
   }
   else
      hasNext = true;

   while ( hasNext )
   {
      std::string parentID = currentContDirID;

      unsigned remainingEntries = maxEntries - readOutEntries;
      StringList entryNames;

      bool parentDirInodeIsTemp = false;

      FileIDLock dirLock;

      if (getIsBuddyMirrored())
         dirLock = {Program::getApp()->getMirroredSessions()->getEntryLockStore(), parentID, false};

      DirInode* parentDirInode = metaStore->referenceDir(parentID, getIsBuddyMirrored(), true);

      // it could be, that parentDirInode does not exist
      // in fsck we create a temporary inode for this case
      if ( unlikely(!parentDirInode) )
      {
         log.log(
            Log_NOTICE,
            "Could not reference directory. EntryID: " + parentID
               + " => using temporary directory inode ");

         // create temporary inode
         int mode = S_IFDIR | S_IRWXU;
         UInt16Vector stripeTargets;
         Raid0Pattern stripePattern(0, stripeTargets, 0);
         parentDirInode = new DirInode(parentID, mode, 0, 0,
            Program::getApp()->getLocalNode().getNumID(), stripePattern, getIsBuddyMirrored());

         parentDirInodeIsTemp = true;
      }

      if ( parentDirInode->listIncremental(lastContDirOffset, remainingEntries, &entryNames,
         &newContDirOffset) == FhgfsOpsErr_SUCCESS )
      {
         lastContDirOffset = newContDirOffset;
         readOutEntries += entryNames.size();
      }
      else
      {
         log.log(Log_WARNING, "Could not list contents of directory. EntryID: " + parentID);
      }

      // actually process the entries; for the dentry part we only need to know if it is a file
      // with inlined inode data
      for ( StringListIter namesIter = entryNames.begin(); namesIter != entryNames.end();
         namesIter++ )
      {
         std::string filename = MetaStorageTk::getMetaDirEntryPath(
               getIsBuddyMirrored()
                  ? Program::getApp()->getBuddyMirrorDentriesPath()->str()
                  : Program::getApp()->getDentriesPath()->str(), parentID) + "/" + *namesIter;

         EntryInfo entryInfo;
         FileInodeStoreData inodeDiskData;

         auto [getEntryRes, isFileOpen] = metaStore->getEntryData(parentDirInode, *namesIter, &entryInfo,
            &inodeDiskData);
         inodeDiskData.setDynamicOrigParentEntryID(parentID);

         if (getEntryRes == FhgfsOpsErr_SUCCESS ||
             getEntryRes == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED  )
         {
            DirEntryType entryType = entryInfo.getEntryType();

            // we only care if inode data is present
            if ( (DirEntryType_ISFILE(entryType)) && (entryInfo.getIsInlined() ) )
            {
               const std::string& inodeID = inodeDiskData.getEntryID();
               unsigned userID = inodeDiskData.getInodeStatData()->getUserID();
               unsigned groupID = inodeDiskData.getInodeStatData()->getGroupID();
               StripePattern* pattern = inodeDiskData.getStripePattern();

               PathInfo pathInfo;
               inodeDiskData.getPathInfo(&pathInfo);

               if ( !this->sendSetAttrMsg(inodeID, userID, groupID, &pathInfo, pattern) )
                  errorCount++;
            }
         }
         else
         {
            log.log(Log_WARNING, "Unable to create dir entry from entry with name " + *namesIter
                  + " in directory with ID " + parentID);
         }
      }

      if ( parentDirInodeIsTemp )
         SAFE_DELETE(parentDirInode);
      else
         metaStore->releaseDir(parentID);

      if ( entryNames.size() < remainingEntries )
      {
         // directory is at the end => proceed with next
         hasNext = StorageTkEx::getNextContDirID(hashDirNum, getIsBuddyMirrored(),
               lastHashDirOffset, &currentContDirID, &newHashDirOffset);

         if ( hasNext )
         {
            lastHashDirOffset = newHashDirOffset;
            lastContDirOffset = 0;
         }
      }
      else
      {
         // there are more to come, but we need to exit the loop now, because maxCount is reached
         hasNext = false;
      }
   }

   ctx.sendResponse(
         AdjustChunkPermissionsRespMsg(readOutEntries, currentContDirID, lastHashDirOffset,
            lastContDirOffset, errorCount) );

   return true;
}

bool AdjustChunkPermissionsMsgEx::sendSetAttrMsg(const std::string& entryID, unsigned userID,
   unsigned groupID, PathInfo* pathInfo, StripePattern* pattern)
{
   const char* logContext = "AdjustChunkPermissionsMsgEx::sendSetAttrMsg";

   MultiWorkQueue* slaveQueue = Program::getApp()->getCommSlaveQueue();

   int validAttribs = SETATTR_CHANGE_USERID | SETATTR_CHANGE_GROUPID; // only interested in these
   SettableFileAttribs attribs;
   attribs.userID = userID;
   attribs.groupID = groupID;

   const UInt16Vector* stripeTargets = pattern->getStripeTargetIDs();
   size_t numTargetWorks = stripeTargets->size();

   FhgfsOpsErrVec nodeResults(numTargetWorks);
   SynchronizedCounter counter;

   // generate work for storage targets...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      SetChunkFileAttribsWork* work = new SetChunkFileAttribsWork(
         entryID, validAttribs, &attribs, false, pattern, (*stripeTargets)[i], pathInfo,
         NULL, &(nodeResults[i]), &counter);

      work->setQuotaChown(true);
      work->setMsgUserID(getMsgHeaderUserID() );

      slaveQueue->addDirectWork(work);
   }

   // wait for work completion...

   counter.waitForCount(numTargetWorks);

   // check target results...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      if(unlikely(nodeResults[i] != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during setting of chunk file attribs. "
            "fileID: " + entryID );

         return false;
      }
   }


   return true;
}
