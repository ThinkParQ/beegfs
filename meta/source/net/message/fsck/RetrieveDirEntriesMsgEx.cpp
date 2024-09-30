#include "RetrieveDirEntriesMsgEx.h"

#include <common/storage/striping/Raid0Pattern.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <program/Program.h>

bool RetrieveDirEntriesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Incoming RetrieveDirEntriesMsg");

   unsigned hashDirNum = getHashDirNum();
   std::string currentContDirID = getCurrentContDirID();
   unsigned maxOutEntries = getMaxOutEntries();

   int64_t lastContDirOffset = getLastContDirOffset();
   int64_t lastHashDirOffset = getLastHashDirOffset();
   int64_t newHashDirOffset;
   int64_t newContDirOffset;

   FsckContDirList contDirsOutgoing;
   FsckDirEntryList dirEntriesOutgoing;
   FsckFileInodeList inlinedFileInodesOutgoing;

   unsigned readOutEntries = 0;

   NumNodeID localNodeNumID = getIsBuddyMirrored()
      ? NumNodeID(Program::getApp()->getMetaBuddyGroupMapper()->getLocalGroupID())
      : Program::getApp()->getLocalNode().getNumID();
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   MirrorBuddyGroupMapper* bgm = Program::getApp()->getMetaBuddyGroupMapper();

   if (getIsBuddyMirrored() &&
         (bgm->getLocalBuddyGroup().secondTargetID
               == Program::getApp()->getLocalNode().getNumID().val()
          || bgm->getLocalGroupID() == 0))
   {
      ctx.sendResponse(
            RetrieveDirEntriesRespMsg(&contDirsOutgoing, &dirEntriesOutgoing,
               &inlinedFileInodesOutgoing, currentContDirID, lastHashDirOffset, lastContDirOffset));
      return true;
   }

   bool hasNext;

   if ( currentContDirID.empty() )
   {
      hasNext = StorageTkEx::getNextContDirID(hashDirNum, getIsBuddyMirrored(), lastHashDirOffset,
         &currentContDirID, &newHashDirOffset);
      if ( hasNext )
      {
         lastHashDirOffset = newHashDirOffset;
         // we found a new .cont directory => send it to fsck
         FsckContDir contDir(currentContDirID, localNodeNumID, getIsBuddyMirrored());
         contDirsOutgoing.push_back(contDir);
      }
   }
   else
      hasNext = true;

   while ( hasNext )
   {
      std::string parentID = currentContDirID;

      unsigned remainingOutNames = maxOutEntries - readOutEntries;
      StringList entryNames;

      bool parentDirInodeIsTemp = false;
      DirInode* parentDirInode = metaStore->referenceDir(parentID, getIsBuddyMirrored(), true);

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
            Program::getApp()->getLocalNode().getNumID(), stripePattern, getIsBuddyMirrored());

         parentDirInodeIsTemp = true;
      }

      if ( parentDirInode->listIncremental(lastContDirOffset, remainingOutNames, &entryNames,
         &newContDirOffset) == FhgfsOpsErr_SUCCESS )
      {
         lastContDirOffset = newContDirOffset;
      }
      else
      {
         log.log(Log_WARNING, "Could not list contents of directory. EntryID: " + parentID);
      }

      // actually process the entries
      for ( StringListIter namesIter = entryNames.begin(); namesIter != entryNames.end();
         namesIter++ )
      {
         std::string filename = MetaStorageTk::getMetaDirEntryPath(
               getIsBuddyMirrored()
                  ? Program::getApp()->getBuddyMirrorDentriesPath()->str()
                  : Program::getApp()->getDentriesPath()->str(), parentID) + "/" + *namesIter;

         // create a EntryInfo and put the information into an FsckDirEntry object
         EntryInfo entryInfo;
         FileInodeStoreData inodeDiskData;
         bool hasInlinedInode = false;

         int32_t saveDevice = 0;
         uint64_t saveInode = 0;

         auto [getEntryRes, isFileOpen] = metaStore->getEntryData(parentDirInode, *namesIter, &entryInfo,
            &inodeDiskData);

         if (getEntryRes == FhgfsOpsErr_SUCCESS ||
             getEntryRes == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED  )
         {
            DirEntryType entryType = entryInfo.getEntryType();

            const std::string& dentryID = entryInfo.getEntryID();
            const std::string& dentryName = *namesIter;
            NumNodeID dentryOwnerID = entryInfo.getOwnerNodeID();
            FsckDirEntryType fsckEntryType = FsckTk::DirEntryTypeToFsckDirEntryType(entryType);

            // stat the file to get device and inode information
            struct stat statBuf;

            int statRes = stat(filename.c_str(), &statBuf);

            if (likely(!statRes))
            {
               saveDevice = statBuf.st_dev;
               saveInode = statBuf.st_ino;
            }
            else
            {
               log.log(Log_CRITICAL, "Could not stat dir entry file; entryID: " + dentryID
                  + ";filename: " + filename);
            }

            if ( (DirEntryType_ISFILE(entryType)) && (entryInfo.getIsInlined() ) )
            {
               hasInlinedInode = true;
            }

            FsckDirEntry fsckDirEntry(dentryID, dentryName, parentID, localNodeNumID,
               dentryOwnerID, fsckEntryType, hasInlinedInode, localNodeNumID,
               saveDevice, saveInode, entryInfo.getIsBuddyMirrored());

            dirEntriesOutgoing.push_back(fsckDirEntry);
         }
         else
         {
            log.log(Log_WARNING, "Unable to create dir entry from entry with name " + *namesIter
                  + " in directory with ID " + parentID);
         }

         // now, if the inode data is inlined we create an fsck inode object here
         if ( hasInlinedInode )
         {
            std::string inodeID = inodeDiskData.getEntryID();

            int pathInfoFlag;
            if (inodeDiskData.getOrigFeature() == FileInodeOrigFeature_TRUE)
               pathInfoFlag = PATHINFO_FEATURE_ORIG;
            else
               pathInfoFlag = PATHINFO_FEATURE_ORIG_UNKNOWN;

            unsigned origUID = inodeDiskData.getOrigUID();
            std::string origParentEntryID = inodeDiskData.getOrigParentEntryID();
            PathInfo pathInfo(origUID, origParentEntryID, pathInfoFlag);

            unsigned userID;
            unsigned groupID;

            int64_t fileSize;
            unsigned numHardLinks;
            uint64_t numBlocks;

            StatData* statData;
            StatData updatedStatData;

            if (getEntryRes == FhgfsOpsErr_SUCCESS)
               statData = inodeDiskData.getInodeStatData();
            else
            {
               FhgfsOpsErr statRes = MsgHelperStat::stat(&entryInfo, true, getMsgHeaderUserID(),
                  updatedStatData);

               if (statRes == FhgfsOpsErr_SUCCESS)
                  statData = &updatedStatData;
               else
                  statData = NULL;
            }

            if ( statData )
            {
               userID = statData->getUserID();
               groupID = statData->getGroupID();
               fileSize = statData->getFileSize();
               numHardLinks = statData->getNumHardlinks();
               numBlocks = statData->getNumBlocks();
            }
            else
            {
               log.logErr(std::string("Unable to get stat data of inlined file inode: ") + inodeID
                     + ". SysErr: " + System::getErrString());
               userID = 0;
               groupID = 0;
               fileSize = 0;
               numHardLinks = 0;
               numBlocks = 0;
            }

            UInt16Vector stripeTargets;
            unsigned chunkSize;
            FsckStripePatternType stripePatternType = FsckTk::stripePatternToFsckStripePattern(
               inodeDiskData.getPattern(), &chunkSize, &stripeTargets);

            FsckFileInode fileInode(inodeID, parentID, localNodeNumID, pathInfo, userID, groupID,
                  fileSize, numHardLinks, numBlocks, stripeTargets, stripePatternType, chunkSize,
                  localNodeNumID, saveInode, saveDevice, true, entryInfo.getIsBuddyMirrored(),
                  true, inodeDiskData.getIsBuddyMirrored() != getIsBuddyMirrored());

            inlinedFileInodesOutgoing.push_back(fileInode);
         }
      }

      if ( parentDirInodeIsTemp )
         SAFE_DELETE(parentDirInode);
      else
         metaStore->releaseDir(parentID);

      if ( entryNames.size() < remainingOutNames )
      {
         // directory is at the end => proceed with next
         hasNext = StorageTkEx::getNextContDirID(hashDirNum, getIsBuddyMirrored(),
               lastHashDirOffset, &currentContDirID, &newHashDirOffset);

         if ( hasNext )
         {
            lastHashDirOffset = newHashDirOffset;
            lastContDirOffset = 0;

            readOutEntries += entryNames.size();

            // we found a new .cont directory => send it to fsck
            FsckContDir contDir(currentContDirID, localNodeNumID, getIsBuddyMirrored());
            contDirsOutgoing.push_back(contDir);
         }
      }
      else
      {
         // there are more to come, but we need to exit the loop now, because maxCount is reached
         hasNext = false;
      }
   }

   ctx.sendResponse(
         RetrieveDirEntriesRespMsg(&contDirsOutgoing, &dirEntriesOutgoing,
            &inlinedFileInodesOutgoing, currentContDirID, lastHashDirOffset, lastContDirOffset) );

   return true;
}
