#include <program/Program.h>
#include <common/net/message/fsck/CheckAndRepairDupInodeRespMsg.h>

#include "CheckAndRepairDupInodeMsgEx.h"

bool CheckAndRepairDupInodeMsgEx::processIncoming(ResponseContext& ctx)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   StringList failedIDList;

   for (const auto& inode : getDuplicateInodes())
   {
      const std::string& entryID = inode.getID();
      const std::string& parentEntryID = inode.getParentDirID();
      const bool isBuddyMirrored = inode.getIsBuddyMirrored();

      FileIDLock dirLock = {entryLockStore, parentEntryID, true};
      FileIDLock fileLock = {entryLockStore, entryID, true};

      EntryInfo fileInfo(NumNodeID(0), parentEntryID, entryID, std::string(""), DirEntryType_REGULARFILE, 0);
      fileInfo.setBuddyMirroredFlag(isBuddyMirrored);

      DirInode* parentDir = metaStore->referenceDir(parentEntryID, isBuddyMirrored, true);
      FhgfsOpsErr repairRes = metaStore->checkAndRepairDupFileInode(*parentDir, &fileInfo);

      if (repairRes != FhgfsOpsErr_SUCCESS)
      {
        failedIDList.push_back(entryID);  
      }

      metaStore->releaseDir(parentDir->getID());
   }

   ctx.sendResponse(CheckAndRepairDupInodeRespMsg(std::move(failedIDList)));
   return true;
}
