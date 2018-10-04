#include "RetrieveFsIDsMsgEx.h"

#include <common/storage/striping/Raid0Pattern.h>
#include <common/threading/SafeRWLock.h>
#include <program/Program.h>

bool RetrieveFsIDsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Incoming RetrieveFsIDsMsg");

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   unsigned hashDirNum = getHashDirNum();
   bool buddyMirrored = getBuddyMirrored();
   std::string currentContDirID = getCurrentContDirID();
   unsigned maxOutIDs = getMaxOutIDs();

   int64_t lastContDirOffset = getLastContDirOffset();
   int64_t lastHashDirOffset = getLastHashDirOffset();
   int64_t newHashDirOffset;

   FsckFsIDList fsIDsOutgoing;

   unsigned readOutIDs = 0;

   NumNodeID localNodeNumID = buddyMirrored
      ? NumNodeID(Program::getApp()->getMetaBuddyGroupMapper()->getLocalGroupID())
      : Program::getApp()->getLocalNode().getNumID();
   MirrorBuddyGroupMapper* bgm = Program::getApp()->getMetaBuddyGroupMapper();

   if (buddyMirrored &&
         (bgm->getLocalBuddyGroup().secondTargetID == app->getLocalNode().getNumID().val()
          || bgm->getLocalGroupID() == 0))
   {
      ctx.sendResponse(
            RetrieveFsIDsRespMsg(&fsIDsOutgoing, currentContDirID, lastHashDirOffset,
               lastContDirOffset));
      return true;
   }

   bool hasNext;

   if ( currentContDirID.empty() )
   {
      hasNext = StorageTkEx::getNextContDirID(hashDirNum, buddyMirrored, lastHashDirOffset,
         &currentContDirID, &newHashDirOffset);
      if ( hasNext )
         lastHashDirOffset = newHashDirOffset;
   }
   else
      hasNext = true;

   while ( hasNext )
   {
      std::string parentID = currentContDirID;
      std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(
         MetaStorageTk::getMetaDirEntryPath(
            buddyMirrored
               ? app->getBuddyMirrorDentriesPath()->str()
               : app->getDentriesPath()->str(),
            parentID));

      bool parentDirInodeIsTemp = false;
      StringList outNames;
      int64_t outNewServerOffset;
      ListIncExOutArgs outArgs(&outNames, NULL, NULL, NULL, &outNewServerOffset);

      FhgfsOpsErr listRes;

      unsigned remainingOutIDs = maxOutIDs - readOutIDs;

      DirInode* parentDirInode = metaStore->referenceDir(parentID, buddyMirrored, true);

      // it could be, that parentDirInode does not exist
      // in fsck we create a temporary inode for this case, so that we can modify the dentry
      // hopefully, the inode itself will get fixed later
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
            Program::getApp()->getLocalNode().getNumID(), stripePattern, buddyMirrored);

         parentDirInodeIsTemp = true;
      }

      listRes = parentDirInode->listIDFilesIncremental(lastContDirOffset, 0, remainingOutIDs,
         outArgs);

      lastContDirOffset = outNewServerOffset;

      if ( parentDirInodeIsTemp )
         SAFE_DELETE(parentDirInode);
      else
         metaStore->releaseDir(parentID);

      if (listRes != FhgfsOpsErr_SUCCESS)
      {
         log.logErr("Could not read dentry-by-ID files; parentID: " + parentID);
      }

      // process entries
      readOutIDs += outNames.size();

      for ( StringListIter iter = outNames.begin(); iter != outNames.end(); iter++ )
      {
         std::string id = *iter;
         std::string filename = idPath + "/" + id;

         // stat the file to get device and inode information
         struct stat statBuf;

         int statRes = stat(filename.c_str(), &statBuf);

         int saveDevice;
         uint64_t saveInode;
         if ( likely(!statRes) )
         {
            saveDevice = statBuf.st_dev;
            saveInode = statBuf.st_ino;
         }
         else
         {
            saveDevice = 0;
            saveInode = 0;
            log.log(Log_CRITICAL,
               "Could not stat ID file; ID: " + id + ";filename: " + filename);
         }

         FsckFsID fsID(id, parentID, localNodeNumID, saveDevice, saveInode, buddyMirrored);
         fsIDsOutgoing.push_back(fsID);
      }

      if ( readOutIDs < maxOutIDs )
      {
         // directory is at the end => proceed with next
         hasNext = StorageTkEx::getNextContDirID(hashDirNum, buddyMirrored, lastHashDirOffset,
            &currentContDirID, &newHashDirOffset);

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
         RetrieveFsIDsRespMsg(&fsIDsOutgoing, currentContDirID, lastHashDirOffset,
            lastContDirOffset) );

   return true;
}
