#include "RecreateDentriesMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckFsID.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool RecreateDentriesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RecreateDentriesMsgEx");

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   EntryLockStore* entryLockStore = app->getMirroredSessions()->getEntryLockStore();

   FsckFsIDList& fsIDs = getFsIDs();
   FsckFsIDList failedCreates;
   FsckDirEntryList createdDentries;
   FsckFileInodeList createdInodes;

   for ( FsckFsIDListIter iter = fsIDs.begin(); iter != fsIDs.end(); iter++ )
   {
      NumNodeID localNodeID = iter->getIsBuddyMirrored()
         ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID())
         : app->getLocalNodeNumID();

      std::string parentPath = MetaStorageTk::getMetaDirEntryPath(
            iter->getIsBuddyMirrored()
               ? app->getBuddyMirrorDentriesPath()->str()
               : app->getDentriesPath()->str(), iter->getParentDirID());

      std::string dirEntryIDFilePath = MetaStorageTk::getMetaDirEntryIDPath(parentPath) + "/"
         + iter->getID();

      // the name is lost, so we take the ID as new name
      std::string dirEntryNameFilePath = parentPath + "/" + iter->getID();

      // before we link, let's see if we can open the parent dir, otherwise we should not mess
      // around here
      const std::string& dirID = iter->getParentDirID();

      FileIDLock dirLock;
      ParentNameLock dentryLock;

      if (iter->getIsBuddyMirrored())
      {
         dirLock = {entryLockStore, dirID, true};
         dentryLock = {entryLockStore, dirID, iter->getID()};
      }

      DirInode* parentDirInode = metaStore->referenceDir(dirID, iter->getIsBuddyMirrored(), false);

      if (!parentDirInode)
      {
         log.logErr("Unable to reference parent directory; ID: " + iter->getParentDirID());
         failedCreates.push_back(*iter);
         continue;
      }

      // link the dentry-by-name file
      int linkRes = link(dirEntryIDFilePath.c_str(), dirEntryNameFilePath.c_str());

      if ( linkRes )
      {
         // error occured while linking
         log.logErr(
            "Failed to link dentry file; ParentID: " + iter->getParentDirID() + "; ID: "
               + iter->getID());
         failedCreates.push_back(*iter);

         metaStore->releaseDir(dirID);
         continue;
      }

      // linking was OK => gather dentry (and inode) data, so fsck can add it

      DirEntry dirEntry(iter->getID());
      bool getRes = parentDirInode->getDentry(iter->getID(), dirEntry);

      if (!getRes)
      {
         log.logErr(
            "Could not read the created dentry file; ParentID: " + iter->getParentDirID() + "; ID: "
               + iter->getID());
         failedCreates.push_back(*iter);

         metaStore->releaseDir(dirID);
         continue;
      }

      // create the FsckDirEntry
      FsckDirEntry fsckDirEntry(dirEntry.getID(), dirEntry.getName(), iter->getParentDirID(),
         localNodeID, localNodeID,
         FsckTk::DirEntryTypeToFsckDirEntryType(dirEntry.getEntryType()), true, localNodeID,
         iter->getSaveDevice(), iter->getSaveInode(), iter->getIsBuddyMirrored());
      createdDentries.push_back(fsckDirEntry);

      // inlined inode data should be present, because otherwise dentry-by-id file would not
      // exist, and we could not get this far
      FileInodeStoreData* inodeData = dirEntry.getInodeStoreData();

      if ( inodeData )
      {
         int pathInfoFlags;

         if (inodeData->getOrigFeature() == FileInodeOrigFeature_TRUE)
            pathInfoFlags = PATHINFO_FEATURE_ORIG;
         else
            pathInfoFlags = PATHINFO_FEATURE_ORIG_UNKNOWN;

         PathInfo pathInfo(inodeData->getOrigUID(), inodeData->getOrigParentEntryID(),
            pathInfoFlags);

         UInt16Vector targetIDs;
         unsigned chunkSize;
         FsckStripePatternType fsckStripePatternType = FsckTk::stripePatternToFsckStripePattern(
            inodeData->getPattern(), &chunkSize, &targetIDs);

         FsckFileInode fsckFileInode(inodeData->getEntryID(), iter->getParentDirID(),
            localNodeID, pathInfo, inodeData->getInodeStatData(),
            inodeData->getInodeStatData()->getNumBlocks(), targetIDs, fsckStripePatternType,
            chunkSize, localNodeID, iter->getSaveInode(), iter->getSaveDevice(), true,
            iter->getIsBuddyMirrored(), true, false);

         createdInodes.push_back(fsckFileInode);
      }
      else
      {
         log.logErr(
            "No inlined inode data found; parentID: " + iter->getParentDirID() + "; ID: "
               + iter->getID());
      }

      metaStore->releaseDir(dirID);
   }

   ctx.sendResponse(RecreateDentriesRespMsg(&failedCreates, &createdDentries, &createdInodes) );

   return true;
}
