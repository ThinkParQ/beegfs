#include "UpdateDirAttribsMsgEx.h"

#include <storage/MetaStore.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool UpdateDirAttribsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "UpdateDirAttribsMsg incoming";

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   FsckDirInodeList& inodes = getInodes();
   FsckDirInodeList failedInodes;

   for (FsckDirInodeListIter iter = inodes.begin(); iter != inodes.end(); iter++)
   {
      // call the updating method
      const std::string& dirID = iter->getID();

      FileIDLock lock;

      if (iter->getIsBuddyMirrored())
         lock = {entryLockStore, dirID, true};

      DirInode* dirInode = metaStore->referenceDir(dirID, iter->getIsBuddyMirrored(), true);

      if (!dirInode)
      {
         LogContext(logContext).logErr("Unable to reference directory; ID: " + dirID);
         failedInodes.push_back(*iter);
         continue;
      }

      FhgfsOpsErr refreshRes = dirInode->refreshMetaInfo();

      metaStore->releaseDir(dirID);

      if (refreshRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(Log_WARNING, "Failed to update attributes of directory. "
            "entryID: " + dirID);
         failedInodes.push_back(*iter);
      }
   }

   ctx.sendResponse(UpdateDirAttribsRespMsg(&failedInodes) );

   return true;
}
