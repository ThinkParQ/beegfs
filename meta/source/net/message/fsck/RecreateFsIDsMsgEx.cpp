#include "RecreateFsIDsMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool RecreateFsIDsMsgEx::processIncoming(ResponseContext& ctx)
{
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   LogContext log("RecreateFsIDsMsgEx");

   FsckDirEntryList& entries = getEntries();
   FsckDirEntryList failedEntries;

   for ( FsckDirEntryListIter iter = entries.begin(); iter != entries.end(); iter++ )
   {
      const std::string& parentID = iter->getParentDirID();
      const std::string& entryName = iter->getName();
      const std::string& entryID = iter->getID();

      std::string dirEntryPath = MetaStorageTk::getMetaDirEntryPath(
            iter->getIsBuddyMirrored()
               ? Program::getApp()->getBuddyMirrorDentriesPath()->str()
               : Program::getApp()->getDentriesPath()->str(), parentID);

      std::string dirEntryIDFilePath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) +
         "/" + entryID;

      std::string dirEntryNameFilePath = dirEntryPath + "/" + entryName;

      FileIDLock dirLock(entryLockStore, parentID, true);
      ParentNameLock dentryLock(entryLockStore, parentID, entryName);
      FileIDLock fileLock(entryLockStore, entryID, true);

      // delete the old dentry-by-id file link (if one existed)
      int removeRes = unlink(dirEntryIDFilePath.c_str());

      if ( (removeRes) && (errno != ENOENT) )
      {
         log.logErr(
            "Failed to recreate dentry-by-id file for directory entry; ParentID: " + parentID
               + "; EntryName: " + entryName
               + " - Could not delete old, faulty dentry-by-id file link");
         failedEntries.push_back(*iter);
         continue;
      }

      // link a new one
      int linkRes = link(dirEntryNameFilePath.c_str(), dirEntryIDFilePath.c_str());

      if ( linkRes )
      {
         log.logErr(
            "Failed to recreate dentry-by-id file for directory entry; ParentID: " + parentID
               + "; EntryName: " + entryName + " - File could not be linked");
         failedEntries.push_back(*iter);
         continue;
      }
   }

   ctx.sendResponse(RecreateFsIDsRespMsg(&failedEntries) );

   return true;
}
