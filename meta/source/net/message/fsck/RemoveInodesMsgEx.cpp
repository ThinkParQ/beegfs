#include <program/Program.h>
#include <common/net/message/fsck/RemoveInodesRespMsg.h>
#include <common/toolkit/ZipIterator.h>
#include <toolkit/BuddyCommTk.h>

#include "RemoveInodesMsgEx.h"


bool RemoveInodesMsgEx::processIncoming(ResponseContext& ctx)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   StringList failedIDList;

   for (auto it = items.begin(); it != items.end(); ++it)
   {
      const std::string& entryID = std::get<0>(*it);
      const DirEntryType entryType = std::get<1>(*it);
      const bool isBuddyMirrored = std::get<2>(*it);
      FhgfsOpsErr rmRes;

      FileIDLock dirLock;
      FileIDLock fileLock;

      if (entryType == DirEntryType_DIRECTORY)
      {
         dirLock = {entryLockStore, entryID, true};
         rmRes = metaStore->removeDirInode(entryID, isBuddyMirrored);
      }
      else
      {
         fileLock = {entryLockStore, entryID, true};
         rmRes = metaStore->fsckUnlinkFileInode(entryID, isBuddyMirrored);
      }

      if (rmRes != FhgfsOpsErr_SUCCESS)
         failedIDList.push_back(entryID);
   }

   ctx.sendResponse(RemoveInodesRespMsg(std::move(failedIDList)));

   return true;
}
