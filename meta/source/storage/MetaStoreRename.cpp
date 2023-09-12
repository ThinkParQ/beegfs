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

#include <boost/lexical_cast.hpp>

/**
 * Simple rename on the same server in the same directory.
 *
 * @param outUnlinkInode is the inode of a dirEntry being possibly overwritten (toName already
 *    existed).
 */
FhgfsOpsErr MetaStore::renameInSameDir(DirInode& parentDir, const std::string& fromName,
   const std::string& toName, std::unique_ptr<FileInode>* outUnlinkInode,
   DirEntry*& outOverWrittenEntry, bool& outUnlinkedWasInlined)
{
   const char* logContext = "Rename in dir";

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K
   SafeRWLock fromMutexLock(&parentDir.rwlock, SafeRWLock_WRITE); // L O C K ( F R O M )

   FhgfsOpsErr retVal;
   FhgfsOpsErr unlinkRes = FhgfsOpsErr_SUCCESS; // initialize just to please compiler

   outOverWrittenEntry = NULL;

   retVal = performRenameEntryInSameDir(parentDir, fromName, toName, &outOverWrittenEntry);

   if (retVal != FhgfsOpsErr_SUCCESS)
   {
      fromMutexLock.unlock();
      safeLock.unlock();

      SAFE_DELETE(outOverWrittenEntry);

      return retVal;
   }

   EntryInfo unlinkEntryInfo;

   // unlink for non-inlined inode will be handled later
   if (outOverWrittenEntry)
   {
      const std::string& parentDirID = parentDir.getID();
      outOverWrittenEntry->getEntryInfo(parentDirID, 0, &unlinkEntryInfo);
      outUnlinkedWasInlined = outOverWrittenEntry->getIsInodeInlined();

      if (outOverWrittenEntry->getIsInodeInlined())
      {
         unlinkRes = unlinkOverwrittenEntryUnlocked(parentDir, outOverWrittenEntry, outUnlinkInode);
      }
      else
      {
         outUnlinkInode->reset();
         unlinkRes = FhgfsOpsErr_SUCCESS;
      }
   }

   /* Now update the ctime (attribChangeTime) of the renamed entry.
    * Only do that for Directory dentry after giving up the DirInodes (fromMutex) lock
    * as dirStore.setAttr() will aquire the InodeDirStore:: lock
    * and the lock order is InodeDirStore:: and then DirInode::  (risk of deadlock) */

   DirEntry* entry = parentDir.dirEntryCreateFromFileUnlocked(toName);
   if (likely(entry) ) // entry was just renamed to, so very likely it exists
   {
      EntryInfo entryInfo;
      const std::string& parentID = parentDir.getID();
      entry->getEntryInfo(parentID, 0, &entryInfo);

      fromMutexLock.unlock();
      setAttrUnlocked(&entryInfo, 0, NULL); /* This will fail if the DirInode is on another
                                             * meta server, but as updating the ctime is not
                                             * a real posix requirement (but filesystems usually
                                             * do it) we simply ignore this issue for now. */

      SAFE_DELETE(entry);
   }
   else
      fromMutexLock.unlock();

   safeLock.unlock();

   // unlink later must be called after releasing all locks

   if (outOverWrittenEntry)
   {
      if (unlinkRes == FhgfsOpsErr_INUSE)
      {
         unlinkRes = unlinkInodeLater(&unlinkEntryInfo, outUnlinkedWasInlined );
         if (unlinkRes == FhgfsOpsErr_AGAIN)
         {
            unlinkRes = unlinkOverwrittenEntry(parentDir, outOverWrittenEntry, outUnlinkInode);
         }
      }

      if (unlinkRes != FhgfsOpsErr_SUCCESS && unlinkRes != FhgfsOpsErr_PATHNOTEXISTS)
      {
         LogContext(logContext).logErr("Failed to unlink overwritten entry:"
            " FileName: "      + toName                         +
            " ParentEntryID: " + parentDir.getID()             +
            " entryID: "       + outOverWrittenEntry->getEntryID() +
            " Error: "         + boost::lexical_cast<std::string>(unlinkRes));


         // TODO: Restore the dentry
      }
   }

   return retVal;
}

/**
 * Unlink an overwritten dentry. From this dentry either the #fsid# entry or its inode is left.
 *
 * Locking:
 *   We lock everything ourself
 */
FhgfsOpsErr MetaStore::unlinkOverwrittenEntry(DirInode& parentDir,
   DirEntry* overWrittenEntry, std::unique_ptr<FileInode>* outInode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K
   SafeRWLock parentLock(&parentDir.rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr unlinkRes = unlinkOverwrittenEntryUnlocked(parentDir, overWrittenEntry, outInode);

   parentLock.unlock();
   safeLock.unlock();

   return unlinkRes;
}

/**
 * See unlinkOverwrittenEntry() for details
 *
 * Locking:
 *    MetaStore rwlock: Read-lock
 *    parentDir       : Write-lock
 */
FhgfsOpsErr MetaStore::unlinkOverwrittenEntryUnlocked(DirInode& parentDir,
   DirEntry* overWrittenEntry, std::unique_ptr<FileInode>* outInode)
{
   FhgfsOpsErr unlinkRes;

   if (overWrittenEntry->getIsInodeInlined() )
   {
      /* We advise the calling code not to try to delete the entryName dentry,
       * as renameEntryUnlocked() already did that */
      unlinkRes = unlinkDirEntryWithInlinedInodeUnlocked("", parentDir, overWrittenEntry,
            DirEntry_UNLINK_ID, outInode);
   }
   else
   {
      // And also do not try to delete the dir-entry-by-name here.
      unlinkRes = unlinkDentryAndInodeUnlocked("", parentDir, overWrittenEntry,
         DirEntry_UNLINK_ID, outInode);
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
 *
 * Note: MetaStore is ReadLocked, dir is WriteLocked
 */
FhgfsOpsErr MetaStore::performRenameEntryInSameDir(DirInode& dir, const std::string& fromName,
   const std::string& toName, DirEntry** outOverwrittenEntry)
{
   *outOverwrittenEntry = NULL;

   FhgfsOpsErr retVal;

   // load DirInode on demand if required, we need it now
   bool loadSuccess = dir.loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // of the file being renamed
   DirEntry* fromEntry  = dir.dirEntryCreateFromFileUnlocked(fromName);
   if (!fromEntry)
   {
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   EntryInfo fromEntryInfo;
   const std::string& parentEntryID = dir.getID();
   fromEntry->getEntryInfo(parentEntryID, 0, &fromEntryInfo);

   // reference the inode
   MetaFileHandle fromFileInode;
   // DirInode*  fromDirInode  = NULL;
   if (DirEntryType_ISDIR(fromEntryInfo.getEntryType() ) )
   {
      // TODO, exclusive lock
   }
   else
   {
      // for nonInlined inode(s) - inode may not be present on local meta server
      // only try to referece file inode for inlined inode(s)
      if (fromEntry->getIsInodeInlined())
      {
         fromFileInode = referenceFileUnlocked(dir, &fromEntryInfo);
         if (!fromFileInode)
         {
            /* Note: The inode might be exclusively locked and a remote rename op might be in progress.
             *       If that fails we should actually continue with our rename. That will be solved
             *       in the future by using hardlinks for remote renaming. */
            return FhgfsOpsErr_PATHNOTEXISTS;
         }
      }
   }

   DirEntry* overWriteEntry = dir.dirEntryCreateFromFileUnlocked(toName);
   if (overWriteEntry)
   {
      // sanity checks if we really shall overwrite the entry

      const std::string& parentID = dir.getID();

      EntryInfo fromEntryInfo;
      fromEntry->getEntryInfo(parentID , 0, &fromEntryInfo);

      EntryInfo overWriteEntryInfo;
      overWriteEntry->getEntryInfo(parentID, 0, &overWriteEntryInfo);

      bool isSameInode;
      retVal = checkRenameOverwrite(&fromEntryInfo, &overWriteEntryInfo, isSameInode);

      if (isSameInode)
      {
         delete(overWriteEntry);
         overWriteEntry = NULL;
         goto out; // nothing to do then, rename request will be silently ignored
      }

      if (retVal != FhgfsOpsErr_SUCCESS)
         goto out; // not allowed for some reasons, return it to the user
   }

   // eventually rename here
   retVal = dir.renameDirEntryUnlocked(fromName, toName, overWriteEntry);

   /* Note: If rename faild and and an existing toName was to be overwritten, we don't need to care
    *       about it, the underlying file system has to handle it. */

   /* Note2: Do not decrease directory link count here, even if we overwrote an entry. We will do
    *        that later on in common unlink code, when we going to unlink the entry from
    *        the #fsIDs# dir.
    */

   if (fromFileInode)
      releaseFileUnlocked(dir, fromFileInode);
   else
   {
      // TODO dir
   }


out:

   if (retVal == FhgfsOpsErr_SUCCESS)
      *outOverwrittenEntry = overWriteEntry;
   else
      SAFE_DELETE(overWriteEntry);

   SAFE_DELETE(fromEntry); // always exists when we are here

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
 * @param overWriteInfo entryInfo of overwritten (and possibly unlinked inode if it was inlined) file
 * by the move operation); the caller is responsible for the deletion of the local file and the
 * corresponding object; may not be NULL
 */
FhgfsOpsErr MetaStore::moveRemoteFileInsert(EntryInfo* fromFileInfo, DirInode& toParent,
      const std::string& newEntryName, const char* buf, uint32_t bufLen,
      std::unique_ptr<FileInode>* outUnlinkedInode, EntryInfo* overWriteInfo, EntryInfo& newFileInfo)
{
   // note: we do not allow newEntry to be a file if the old entry was a directory (and vice versa)
   const char* logContext = "rename(): Insert remote entry";

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   outUnlinkedInode->reset();

   SafeRWLock safeMetaStoreLock(&rwlock, SafeRWLock_READ); // L O C K

   // toParent exists
   SafeRWLock toParentMutexLock(&toParent.rwlock, SafeRWLock_WRITE); // L O C K ( T O )


   DirEntry* overWrittenEntry = toParent.dirEntryCreateFromFileUnlocked(newEntryName);
   if (overWrittenEntry)
   {
      const std::string& parentID = overWrittenEntry->getID();
      overWrittenEntry->getEntryInfo(parentID, 0, overWriteInfo);
      bool isSameInode;

      FhgfsOpsErr checkRes = checkRenameOverwrite(fromFileInfo, overWriteInfo, isSameInode);

      if ((checkRes != FhgfsOpsErr_SUCCESS)  || ((checkRes == FhgfsOpsErr_SUCCESS) && isSameInode) )
      {
         retVal = checkRes;
         goto outUnlock;
      }

      // only unlink the dir-entry-name here, so we can still restore it from dir-entry-id
      FhgfsOpsErr unlinkRes = toParent.unlinkDirEntryUnlocked(newEntryName, overWrittenEntry,
         DirEntry_UNLINK_FILENAME);
      if (unlikely(unlinkRes != FhgfsOpsErr_SUCCESS) )
      {
         if (unlikely (unlinkRes == FhgfsOpsErr_PATHNOTEXISTS) )
            LogContext(logContext).log(Log_WARNING, "Unexpectedly failed to unlink file: " +
               toParent.entries.getDirEntryPathUnlocked() + newEntryName + ". ");
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
      Deserializer des(buf, bufLen);
      inode->deserializeMetaData(des);
      if (!des.good())
      {
         LogContext("File rename").logErr("Bug: Deserialization of remote buffer failed. Are all "
            "meta servers running with the same version?" );
         retVal = FhgfsOpsErr_INTERNAL;

         delete inode;
         goto outUnlock;
      }

      // ensure that the buddyMirrored flag of the created inode is set correctly. the source could
      // check this as well, but since we already have the destination dir inode, we are in a better
      // position to do this.
      if (toParent.getIsBuddyMirrored())
         inode->setIsBuddyMirrored();
      else
         inode->setIsBuddyMirrored(false);

      // destructs inode
      retVal = mkMetaFileUnlocked(toParent, newEntryName, fromFileInfo, inode);
   }

   if (retVal == FhgfsOpsErr_SUCCESS)
   {
      if (!toParent.entries.getFileEntryInfo(newEntryName, newFileInfo))
         retVal = FhgfsOpsErr_INTERNAL;
   }

   if (overWrittenEntry && overWrittenEntry->getIsInodeInlined() && (retVal == FhgfsOpsErr_SUCCESS))
   {
      // unlink overwritten entry if it had an inlined inode (non-inlined inodes will be unlinked later)
      bool unlinkedWasInlined = overWrittenEntry->getIsInodeInlined();

      FhgfsOpsErr unlinkRes = unlinkOverwrittenEntryUnlocked(toParent, overWrittenEntry,
         outUnlinkedInode);

      EntryInfo unlinkEntryInfo;
      overWrittenEntry->getEntryInfo(toParent.getID(), 0, &unlinkEntryInfo);

      // unlock everything here, but do not release toParent yet.
      toParentMutexLock.unlock(); // U N L O C K ( T O )
      safeMetaStoreLock.unlock();

      // unlinkInodeLater() requires that everything was unlocked!
      if (unlinkRes == FhgfsOpsErr_INUSE)
      {
         unlinkRes = unlinkInodeLater(&unlinkEntryInfo, unlinkedWasInlined);
         if (unlinkRes == FhgfsOpsErr_AGAIN)
            unlinkRes = unlinkOverwrittenEntry(toParent, overWrittenEntry, outUnlinkedInode);

         if (unlinkRes != FhgfsOpsErr_SUCCESS && unlinkRes != FhgfsOpsErr_PATHNOTEXISTS)
            LogContext(logContext).logErr("Failed to unlink overwritten entry:"
               " FileName: "      + newEntryName                   +
               " ParentEntryID: " + toParent.getID()              +
               " entryID: "       + overWrittenEntry->getEntryID() +
               " Error: "         + boost::lexical_cast<std::string>(unlinkRes));
      }

      delete overWrittenEntry;
      return retVal;
   }
   else
   if (overWrittenEntry)
   {
      // TODO: Restore the overwritten entry
   }

outUnlock:

   toParentMutexLock.unlock(); // U N L O C K ( T O )

   safeMetaStoreLock.unlock();

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
FhgfsOpsErr MetaStore::moveRemoteFileBegin(DirInode& dir, EntryInfo* entryInfo,
   char* buf, size_t bufLen, size_t* outUsedBufLen)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   SafeRWLock safeLock(&this->rwlock, SafeRWLock_READ); // L O C K

   // lock the dir to make sure no renameInSameDir is going on
   SafeRWLock safeDirLock(&dir.rwlock, SafeRWLock_READ);

   if (entryInfo->getIsInlined())
   {
      if (this->fileStore.isInStore(entryInfo->getEntryID()))
         retVal = this->fileStore.moveRemoteBegin(entryInfo, buf, bufLen, outUsedBufLen);
      else
         retVal = dir.fileStore.moveRemoteBegin(entryInfo, buf, bufLen, outUsedBufLen);
   }
   else
   {
      // handle dentry for a non-inlined inode
      DirEntry fileDentry(entryInfo->getFileName());
      if (!dir.getDentryUnlocked(entryInfo->getFileName(), fileDentry))
         return FhgfsOpsErr_PATHNOTEXISTS;
      else
         retVal = FhgfsOpsErr_SUCCESS;

      Serializer ser(buf, bufLen);
      fileDentry.serializeDentry(ser);
      *outUsedBufLen = ser.size();
   }

   safeDirLock.unlock();
   safeLock.unlock(); // U N L O C K

   return retVal;
}

void MetaStore::moveRemoteFileComplete(DirInode& dir, const std::string& entryID)
{
   SafeRWLock safeLock(&this->rwlock, SafeRWLock_WRITE); // L O C K

   if (this->fileStore.isInStore(entryID) )
      this->fileStore.moveRemoteComplete(entryID);
   else
   {
      SafeRWLock safeDirLock(&dir.rwlock, SafeRWLock_READ);

      dir.fileStore.moveRemoteComplete(entryID);

      safeDirLock.unlock();

   }

   safeLock.unlock(); // U N L O C K
}
