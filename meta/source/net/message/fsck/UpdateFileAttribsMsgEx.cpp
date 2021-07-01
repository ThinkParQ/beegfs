#include "UpdateFileAttribsMsgEx.h"

#include <net/msghelpers/MsgHelperStat.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool UpdateFileAttribsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "UpdateFileAttribsMsg incoming";

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   FsckFileInodeList& inodes = getInodes();
   FsckFileInodeList failedInodes;

   for (FsckFileInodeListIter iter = inodes.begin(); iter != inodes.end(); iter++)
   {
      // create an EntryInfo object (NOTE: dummy fileName)
      EntryInfo entryInfo(iter->getSaveNodeID(), iter->getParentDirID(), iter->getID(), "",
            DirEntryType_REGULARFILE,
            (iter->getIsBuddyMirrored() ? ENTRYINFO_FEATURE_BUDDYMIRRORED : 0) |
            (iter->getIsInlined() ? ENTRYINFO_FEATURE_INLINED : 0));

      FileIDLock lock;

      if (iter->getIsBuddyMirrored())
         lock = {entryLockStore, entryInfo.getEntryID(), true};

      MetaFileHandle inode = metaStore->referenceFile(&entryInfo);

      if (inode)
      {
         inode->setNumHardlinksUnpersistent(iter->getNumHardLinks());
         inode->updateInodeOnDisk(&entryInfo);

         // call the dynamic attribs refresh method
         FhgfsOpsErr refreshRes = MsgHelperStat::refreshDynAttribs(&entryInfo, true,
            getMsgHeaderUserID() );
         if (refreshRes != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).log(Log_WARNING, "Failed to update dynamic attributes of file. "
               "entryID: " + iter->getID());
            failedInodes.push_back(*iter);
         }

         /* only release it here, as refreshDynAttribs() also takes an inode reference and can
          * do the reference from in-memory data then */
         metaStore->releaseFile(entryInfo.getParentEntryID(), inode);
      }
      else
      {
         LogContext(logContext).log(Log_WARNING, "Could not reference inode to update attributes. "
            "entryID: " + iter->getID());
         failedInodes.push_back(*iter);
      }
   }

   ctx.sendResponse(UpdateFileAttribsRespMsg(&failedInodes) );

   return true;
}
