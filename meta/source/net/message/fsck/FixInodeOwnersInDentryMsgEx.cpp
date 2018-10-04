#include "FixInodeOwnersInDentryMsgEx.h"

#include <common/storage/striping/Raid0Pattern.h>
#include <common/fsck/FsckDirEntry.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool FixInodeOwnersInDentryMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("FixInodeOwnersInDentryMsgEx");

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   FsckDirEntryList& dentries = getDentries();
   FsckDirEntryList failedEntries;

   FsckDirEntryListIter dentryIter = dentries.begin();
   NumNodeIDListIter ownerIter = getOwners().begin();

   while (dentryIter != dentries.end() )
   {
      const std::string& parentID = dentryIter->getParentDirID();
      const std::string& entryName = dentryIter->getName();

      ParentNameLock lock;

      if (dentryIter->getIsBuddyMirrored())
         lock = {entryLockStore, parentID, entryName};

      bool parentDirInodeIsTemp = false;
      DirInode* parentDirInode = metaStore->referenceDir(parentID,
            dentryIter->getIsBuddyMirrored(), true);

      // it could be, that parentDirInode does not exist
      // in fsck we create a temporary inode for this case, so that we can modify the dentry
      // hopefully, the inode itself will get fixed later
      if (unlikely(!parentDirInode))
      {
         log.log(Log_NOTICE,
            "Failed to update directory entry. Parent directory could not be "
               "referenced. parentID: " + parentID + " entryName: " + entryName
               + " => Using temporary inode");

         // create temporary inode
         int mode = S_IFDIR | S_IRWXU;
         UInt16Vector stripeTargets;
         Raid0Pattern stripePattern(0, stripeTargets, 0);
         parentDirInode = new DirInode(parentID, mode, 0, 0,
            Program::getApp()->getLocalNode().getNumID(), stripePattern,
            dentryIter->getIsBuddyMirrored());

         parentDirInodeIsTemp = true;
      }

      FhgfsOpsErr updateRes = parentDirInode->setOwnerNodeID(entryName, *ownerIter);

      if (updateRes != FhgfsOpsErr_SUCCESS )
      {
         log.log(Log_WARNING, "Failed to update directory entry. parentID: " + parentID +
            " entryName: " + entryName);
         failedEntries.push_back(*dentryIter);
      }

      if (parentDirInodeIsTemp)
         SAFE_DELETE(parentDirInode);
      else
         metaStore->releaseDir(parentID);

      dentryIter++;
      ownerIter++;
   }

   ctx.sendResponse(FixInodeOwnersInDentryRespMsg(&failedEntries) );

   return true;
}
