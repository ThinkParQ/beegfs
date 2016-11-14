/*
 * MetaStoreRenameHelper.cpp
 *
 * These methods belong to class MetaStore, but are all related to rename()
 */

#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/FsckTk.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <program/Program.h>
#include "MetaStore.h"

#include <sys/types.h>
#include <dirent.h>
#include "MetaStore.h"

/**
 * Simple rename on the same server in the same directory.
 *
 * @param outUnlinkInode is the inode of a dirEntry being possibly overwritten (toName already
 *    existed).
 */
FhgfsOpsErr MetaStore::renameFileInSameDir(DirInode* parentDir, std::string fromName,
   std::string toName, FileInode** outUnlinkInode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   SafeRWLock fromMutexLock(&parentDir->rwlock, SafeRWLock_WRITE); // L O C K ( F R O M )

   FhgfsOpsErr retVal;
   FhgfsOpsErr unlinkRes;

   DirEntry* overWrittenEntry = NULL;

   retVal = performRenameEntryInSameDir(parentDir, fromName, toName, &overWrittenEntry);

   EntryInfo unlinkEntryInfo;
   bool unlinkedWasInlined;

   if (overWrittenEntry)
   {
      std::string parentDirID = parentDir->getID();
      overWrittenEntry->getEntryInfo(parentDirID, 0, &unlinkEntryInfo);

      unlinkRes = unlinkOverwrittenEntryUnlocked(parentDir, overWrittenEntry,
         outUnlinkInode, unlinkedWasInlined);
   }
   else
   {
      *outUnlinkInode = NULL;

      // irrelevant values, just to please the compiler
      unlinkRes = FhgfsOpsErr_SUCCESS;
      unlinkedWasInlined = true;
   }


   fromMutexLock.unlock();

   safeLock.unlock();

   // unlink later must be called after releasing all locks

   if (overWrittenEntry && unlinkRes == FhgfsOpsErr_INUSE)
   {
      unlinkInodeLater(&unlinkEntryInfo, unlinkedWasInlined );
   }

   SAFE_DELETE(overWrittenEntry);

   return retVal;
}

FhgfsOpsErr MetaStore::unlinkOverwrittenEntryUnlocked(DirInode* parentDir,
   DirEntry* overWrittenEntry, FileInode** outInode, bool& outUnlinkedWasInlined)
{
   FhgfsOpsErr unlinkRes;

   if (overWrittenEntry->isInodeInlined() )
   {
      /* We advise the calling code not to try to delete the entryName dentry,
       * as renameEntryUnlocked() already did that */
      unlinkRes = unlinkDirEntryWithInlinedInodeUnlocked("", parentDir, overWrittenEntry, outInode,
         false);

      outUnlinkedWasInlined = true;
   }
   else
   {
      // And also do not try to delete the dir-entry-by-name here.
      unlinkRes = unlinkDentryAndInodeUnlocked("", parentDir, overWrittenEntry, false,
         outInode);

      outUnlinkedWasInlined = false;
   }

   return unlinkRes;
}

/**
 * Perform the rename action here.
 *
 * In constrast to the moving...()-methods, this method performs a simple rename of an entry,
 * where no moving is involved.
 *
 * Rules: Files can overwrite existing files, but not existing dirs. Dirs cannot overwrite any
 * existing entry.
 *
 * @param dir needs to write-locked already
 * @param outOverwrittenEntry the caller is responsible for the deletion of the local file;
 *    accoring to the rules, this can only be an overwritten file, not a dir; may not be NULL.
 *    Also, we only overwrite the entryName dentry, but not the ID dentry.
 */
FhgfsOpsErr MetaStore::performRenameEntryInSameDir(DirInode* dir, std::string fromName,
   std::string toName, DirEntry** outOverwrittenEntry)
{
   *outOverwrittenEntry = NULL;

   FhgfsOpsErr retVal;

   // load DirInode on demand if required, we need it now
   bool loadSuccess = dir->loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // first create information about the file possibly being overwritten
   DirEntry* fromEntry  = dir->dirEntryCreateFromFileUnlocked(fromName);
   if (!fromEntry)
   {
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   // sanity checks

   DirEntry* overWriteEntry = dir->dirEntryCreateFromFileUnlocked(toName);
   if (overWriteEntry)
   {
      std::string parentID = dir->getID();

      EntryInfo fromEntryInfo;
      fromEntry->getEntryInfo(parentID , 0, &fromEntryInfo);

      EntryInfo overWriteEntryInfo;
      overWriteEntry->getEntryInfo(parentID, 0, &overWriteEntryInfo);

      bool isSameInode;
      retVal = checkRenameOverwrite(&fromEntryInfo, &overWriteEntryInfo, isSameInode);

      if (isSameInode)
         goto outErr; // nothing to do them, rename request will be silently ignored

      if (retVal != FhgfsOpsErr_SUCCESS)
         goto outErr; // not allowed for some reasons, return it to the user
   }

   // eventually rename here
   retVal = dir->renameDirEntryUnlocked(fromName, toName);

   // now update the ctime (attribChangeTime) of the renamed inode/dir
   if (retVal == FhgfsOpsErr_SUCCESS)
   {
      DirEntry* entry = dir->dirEntryCreateFromFileUnlocked(toName);

      EntryInfo entryInfo;
      std::string parentID = dir->getID();
      entry->getEntryInfo(parentID, 0, &entryInfo);

      bool isDir = DirEntryType_ISDIR(entryInfo.getEntryType() );

      if (isDir)
         this->dirStore.setAttr(entryInfo.getEntryID(), 0, NULL);
      else
      {
         if (this->fileStore.isInStore(entryInfo.getEntryID() ) )
            this->fileStore.setAttr(&entryInfo, 0, NULL);
         else
         {
            // not in global /store/map, now per directory
            dir->fileStore.setAttr(&entryInfo, 0, NULL);
         }
      }

      delete entry;
   }


   // FIXME Bernd: restore old entryName if retVal != FhgfsOpsErr_SUCCESS

   /* Note: Do not decrease directory link count here, even if we overwrote an entry. We will do
    *       that later on in common unlink code, when we going to unlink the entry from
    *       the #fsIDs# dir.
    */

   delete fromEntry;
   *outOverwrittenEntry = overWriteEntry;
   return retVal;

outErr:
   delete fromEntry; // always exists when we are here
   SAFE_DELETE(overWriteEntry);

   return retVal;
}

/**
 * Check if overwriting an entry on rename is allowed.
 */
FhgfsOpsErr MetaStore::checkRenameOverwrite(EntryInfo* fromEntry, EntryInfo* overWriteEntry,
   bool& outIsSameInode)
{
   outIsSameInode = false;

   // check if we are going to rename to a dentry with the same inode
  if (fromEntry->getEntryID() == overWriteEntry->getEntryID() )
   { // According to posix we must not do anything and return success.

     outIsSameInode = true;
     return FhgfsOpsErr_SUCCESS;
   }

  if (overWriteEntry->getEntryType() == DirEntryType_DIRECTORY)
  {
     return FhgfsOpsErr_EXISTS;
  }

  /* TODO: We should allow this if overWriteEntry->getEntryType() == DirEntryType_DIRECTORY
   *       and overWriteEntry is empty.
   */
  if (fromEntry->getEntryType() == DirEntryType_DIRECTORY)
  {
     return FhgfsOpsErr_EXISTS;
  }

  return FhgfsOpsErr_SUCCESS;
}

/**
 * Create a new file on this (remote) meta-server. This is the 'toFile' on a rename() client call.
 *
 * Note: Replaces existing entry.
 *
 * @param buf serialized inode object
 * @param outUnlinkedInode the unlinked (owned) file (in case a file was overwritten
 * by the move operation); the caller is responsible for the deletion of the local file and the
 * corresponding object; may not be NULL
 */
FhgfsOpsErr MetaStore::moveRemoteFileInsert(EntryInfo* fromFileInfo, std::string toParentID,
   std::string newEntryName, const char* buf, FileInode** outUnlinkedInode)
{
   // note: we do not allow newEntry to be a file if the old entry was a directory (and vice versa)
   const char* logContext = "rename(): Insert remote entry";

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   *outUnlinkedInode = NULL;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DirInode* toParent = referenceDirUnlocked(toParentID, true);
   if(!toParent)
   {
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
      safeLock.unlock(); // U N L O C K
      return retVal;
   }

   // toParent exists
   SafeRWLock toParentMutexLock(&toParent->rwlock, SafeRWLock_WRITE); // L O C K ( T O )


   DirEntry* overWrittenEntry = toParent->dirEntryCreateFromFileUnlocked(newEntryName);
   if (overWrittenEntry)
   {
      EntryInfo overWriteInfo;
      std::string parentID = overWrittenEntry->getID();
      overWrittenEntry->getEntryInfo(parentID, 0, &overWriteInfo);
      bool isSameInode;

      FhgfsOpsErr checkRes = checkRenameOverwrite(fromFileInfo, &overWriteInfo, isSameInode);

      if ((checkRes != FhgfsOpsErr_SUCCESS)  || ((checkRes == FhgfsOpsErr_SUCCESS) && isSameInode) )
      { // same inode, do nothing (to be posix compliant)
         retVal = checkRes;
         goto outUnlock;
      }

      // only unlink the dir-entry-name here, so we can still restore it from dir-entry-id
      FhgfsOpsErr unlinkRes = toParent->entries.unlinkDirEntryName(newEntryName);
      if (unlikely(unlinkRes != FhgfsOpsErr_SUCCESS) )
      {
         if (unlikely (unlinkRes == FhgfsOpsErr_PATHNOTEXISTS) )
            LogContext(logContext).log(Log_WARNING, "Unexpectedly failed to unlink file: " +
               toParent->entries.getDirEntryPathUnlocked() + newEntryName + ". ");
         else
         {
            LogContext(logContext).logErr("Failed to unlink existing file. Aborting rename().");
            retVal = unlinkRes;
            goto outUnlock;
         }
      }
   }

   { // create new dirEntry with inlined inode
      FileInode* inode = new FileInode(); // the deserialized inode
      bool deserializeRes = inode->deserializeMetaData(buf);
      if (deserializeRes == false)
      {
         LogContext("File rename").logErr("Bug: Deserialization of remote buffer failed. Are all "
            "meta servers running with the same version?" );
         retVal = FhgfsOpsErr_INTERNAL;

         delete inode;
         goto outUnlock;
      }

      // destructs inode
      retVal = mkMetaFileUnlocked(toParent, newEntryName, fromFileInfo->getEntryType(), inode);
   }

   if (overWrittenEntry && retVal == FhgfsOpsErr_SUCCESS)
   { // unlink the overwritten entry, will unlock, release and return
      bool unlinkedWasInlined;
      FhgfsOpsErr unlinkRes = unlinkOverwrittenEntryUnlocked(toParent, overWrittenEntry,
         outUnlinkedInode, unlinkedWasInlined);

      std::string parentEntryID = toParent->getID();
      EntryInfo unlinkEntryInfo;
      overWrittenEntry->getEntryInfo(parentEntryID, 0, &unlinkEntryInfo);

      // unlock and release everything here
      toParentMutexLock.unlock(); // U N L O C K ( T O )
      dirStore.releaseDir(parentEntryID);
      safeLock.unlock();

      // unlinkInodeLater() requires that everything was unlocked!
      if (overWrittenEntry && unlinkRes == FhgfsOpsErr_INUSE)
         unlinkInodeLater(&unlinkEntryInfo, unlinkedWasInlined );

      delete overWrittenEntry;

      return retVal;
   }

outUnlock:

   toParentMutexLock.unlock(); // U N L O C K ( T O )

   dirStore.releaseDir(toParent->getID() );

   safeLock.unlock();

   SAFE_DELETE(overWrittenEntry);

   return retVal;
}


/**
 * Copies (serializes) the original file object to a buffer.
 *
 * Note: This works by inserting a temporary placeholder and returning the original, so remember to
 * call movingComplete()
 *
 * @param buf target buffer for serialization
 * @param bufLen must be at least META_SERBUF_SIZE
 */
FhgfsOpsErr MetaStore::moveRemoteFileBegin(DirInode* dir, EntryInfo* entryInfo,
   char* buf, size_t bufLen, size_t* outUsedBufLen)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   SafeRWLock safeLock(&this->rwlock, SafeRWLock_READ); // L O C K

   if (this->fileStore.isInStore(entryInfo->getEntryID() ) )
      retVal = this->fileStore.moveRemoteBegin(entryInfo, buf, bufLen, outUsedBufLen);
   else
   {
      SafeRWLock safeDirLock(&dir->rwlock, SafeRWLock_READ);

      retVal = dir->fileStore.moveRemoteBegin(entryInfo, buf, bufLen, outUsedBufLen);

      safeDirLock.unlock();
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}

void MetaStore::moveRemoteFileComplete(DirInode* dir, std::string entryID)
{
   SafeRWLock safeLock(&this->rwlock, SafeRWLock_WRITE); // L O C K

   if (this->fileStore.isInStore(entryID) )
      this->fileStore.moveRemoteComplete(entryID);
   else
   {
      SafeRWLock safeDirLock(&dir->rwlock, SafeRWLock_READ);

      dir->fileStore.moveRemoteComplete(entryID);

      safeDirLock.unlock();

   }

   safeLock.unlock(); // U N L O C K
}


