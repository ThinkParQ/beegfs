#include "FixInodeOwnersMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool FixInodeOwnersMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "FixInodeOwnersMsgEx incoming";

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   FsckDirInodeList& inodes = getInodes();
   FsckDirInodeList failedInodes;

   for ( FsckDirInodeListIter iter = inodes.begin(); iter != inodes.end(); iter++ )
   {
      const std::string& entryID = iter->getID();
      NumNodeID ownerNodeID = iter->getOwnerNodeID();

      FileIDLock lock;

      if (iter->getIsBuddyMirrored())
         lock = {entryLockStore, entryID, true};

      DirInode* dirInode = metaStore->referenceDir(entryID, iter->getIsBuddyMirrored(), true);

      if (unlikely(!dirInode))
      {
         LogContext(logContext).log(Log_WARNING, "Failed to update directory inode. Inode could"
            " not be referenced. entryID: " + entryID);
         continue; // continue to next entry
      }

      bool updateRes = dirInode->setOwnerNodeID(ownerNodeID);

      metaStore->releaseDir(entryID);

      if (!updateRes)
      {
         LogContext(logContext).log(Log_WARNING, "Failed to update directory inode. entryID: "
            + entryID);
         failedInodes.push_back(*iter);
      }
   }

   ctx.sendResponse(FixInodeOwnersRespMsg(&failedInodes) );

   return true;
}
