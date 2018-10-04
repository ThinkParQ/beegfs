#include <program/Program.h>
#include <toolkit/StorageTkEx.h>
#include "DirEntryStore.h"

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Get a dir-entry-path
 *
 * Note: Not supposed to be used outside of this file and shall be inlined
 */
static inline std::string getDirEntryStoreDynamicEntryPath(const std::string& parentID,
   bool isBuddyMirrored)
{
   App* app = Program::getApp();

   if ( isBuddyMirrored )
      return MetaStorageTk::getMetaDirEntryPath(
         app->getBuddyMirrorDentriesPath()->str(), parentID);
   else
      return MetaStorageTk::getMetaDirEntryPath(app->getDentriesPath()->str(),
         parentID);
}


/**
 * Note: Sets the parentID to an invalid value, so do not forget to set the parentID before
 * adding any elements.
 */
DirEntryStore::DirEntryStore() :
   parentID("<undef>"), isBuddyMirrored(false)
{
}

/**
 * @param parentID ID of the directory to which this store belongs
 * @param isBuddyMirrored true if the directory to which this store belongs is buddy mirrored
 */
DirEntryStore::DirEntryStore(const std::string& parentID, bool isBuddyMirrored) :
   parentID(parentID), dirEntryPath(getDirEntryStoreDynamicEntryPath(parentID, isBuddyMirrored) ),
   isBuddyMirrored(isBuddyMirrored)
{
}

/*
 * Create the new directory for dentries (dir-entries). This directory will contain directory
 * entries of files and sub-directories of the directory given by dirID.
 */
FhgfsOpsErr DirEntryStore::mkDentryStoreDir(const std::string& dirID, bool isBuddyMirrored)
{
   const char* logContext = "Directory (store initial metadata dir)";

   App* app = Program::getApp();

   const Path* dentriesPath =
      isBuddyMirrored ? app->getBuddyMirrorDentriesPath() : app->getDentriesPath();

   std::string contentsDirStr = MetaStorageTk::getMetaDirEntryPath(
      dentriesPath->str(), dirID);

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   // create (contents) directory, which will hold directory entries of subdirs and subfiles

   int mkDirRes = mkdir(contentsDirStr.c_str(), 0755);
   if(mkDirRes)
   { // error
      if(errno == EEXIST)
         retVal = FhgfsOpsErr_EXISTS;
      else
      {
         LogContext(logContext).logErr("Unable to create contents directory: " + contentsDirStr  +
            ". " + "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;
      }

      return retVal;
   }

   LOG_DEBUG(logContext, 4, "Metadata dir created: " + contentsDirStr);

   // create the dirEntryID directory, which allows access to inlined inodes via dirID access

   std::string contentsDirIDStr = MetaStorageTk::getMetaDirEntryIDPath(contentsDirStr);

   int mkDirIDRes = mkdir(contentsDirIDStr.c_str(), 0755);
   if(mkDirIDRes)
   {     // So contentsDirStr worked, but creating the sub directory contentsDirIDStr failed

         LogContext(logContext).logErr("Unable to create dirEntryID directory: " +
            contentsDirIDStr + ". " + "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;

         int unlinkRes = unlink(contentsDirStr.c_str() );
         if (unlinkRes)
         {
            // we can only write a log message here, but can't do anything about it
            LogContext(logContext).logErr("Failed to remove: " + contentsDirStr  +
               ". " + "SysErr: " + System::getErrString() );
         }

      return retVal;
   }

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         resync->addModification(contentsDirStr, MetaSyncFileType::Directory);
         resync->addModification(contentsDirIDStr, MetaSyncFileType::Directory);
      }

   return retVal;
}

/**
 * Remove directory for dir-entries (dentries)
 *
 * Note: Assumes that the caller already verified that the directory is empty
 */
bool DirEntryStore::rmDirEntryStoreDir(const std::string& id, bool isBuddyMirrored)
{
   const char* logContext = "Directory (remove contents dir)";

   App* app = Program::getApp();

   const Path* dentriesPath =
      isBuddyMirrored ? app->getBuddyMirrorDentriesPath() : app->getDentriesPath();

   std::string contentsDirStr = MetaStorageTk::getMetaDirEntryPath(
      dentriesPath->str(), id);

   std::string contentsDirIDStr = MetaStorageTk::getMetaDirEntryIDPath(contentsDirStr);

   LOG_DEBUG(logContext, Log_DEBUG,
      "Removing content directory: " + contentsDirStr + "; " "id: " + id + "; isBuddyMirrored: "
      + StringTk::intToStr(isBuddyMirrored));

   // remove the dirEntryID directory
   int rmdirIdRes = rmdir(contentsDirIDStr.c_str() );
   if(rmdirIdRes)
   { // error
      LogContext(logContext).logErr("Unable to delete dirEntryID directory: " + contentsDirIDStr  +
         ". " + "SysErr: " + System::getErrString() );

      if (errno != ENOENT)
         return false;
   }

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addDeletion(contentsDirIDStr, MetaSyncFileType::Directory);

   // remove contents directory

   int rmdirRes = rmdir(contentsDirStr.c_str() );
   if(rmdirRes)
   { // error
      LogContext(logContext).logErr("Unable to delete contents directory: " + contentsDirStr  +
         ". " + "SysErr: " + System::getErrString() );

      if (errno != ENOENT)
         return false;
   }

   LOG_DEBUG(logContext, 4, "Contents directory deleted: " + contentsDirStr);

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addDeletion(contentsDirStr, MetaSyncFileType::Directory);

   return true;
}


/**
 * @param file belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 */
FhgfsOpsErr DirEntryStore::makeEntry(DirEntry* entry)
{
   FhgfsOpsErr mkRes;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   mkRes = makeEntryUnlocked(entry);

   safeLock.unlock();

   return mkRes;
}

/**
 * @param file belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 */
FhgfsOpsErr DirEntryStore::makeEntryUnlocked(DirEntry* entry)
{
   const std::string& dirEntryPath = getDirEntryPathUnlocked();
   const char* logContext = "make meta dir-entry";

   FhgfsOpsErr mkRes = entry->storeInitialDirEntry(dirEntryPath);

   if (unlikely(mkRes != FhgfsOpsErr_SUCCESS) && mkRes != FhgfsOpsErr_EXISTS)
      LogContext(logContext).logErr(std::string("Failed to create: name: ") + entry->getName() +
         std::string(" entryID: ") + entry->getID() + " in path: " + dirEntryPath);

   return mkRes;
}

/**
 * Create a new dir-entry based on an inode, which is stored in dir-entry format
 * (with inlined inode)
 */
FhgfsOpsErr DirEntryStore::linkInodeToDir(const std::string& inodePath, const std::string &fileName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr retVal = linkInodeToDirUnlocked(inodePath, fileName);

   safeLock.unlock();

   return retVal;
}


FhgfsOpsErr DirEntryStore::linkInodeToDirUnlocked(const std::string& inodePath,
   const std::string& fileName)
{
   const char* logContext = "make meta dir-entry";
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   std::string dirEntryPath = getDirEntryPathUnlocked() + '/' + fileName;

   int linkRes = link(inodePath.c_str(), dirEntryPath.c_str() );
   if (linkRes)
   {
      LogContext(logContext).logErr(std::string("Failed to create link from: ") + inodePath +
         " To: " + dirEntryPath + " SysErr: " + System::getErrString() );
      retVal = FhgfsOpsErr_INTERNAL;
   }

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(dirEntryPath, MetaSyncFileType::Inode);

   return retVal;
}

/**
 * note: only for dir dentries
 *
 * @param outDirEntry is object of the removed dirEntry, maybe NULL of the caller does not need it
 */
FhgfsOpsErr DirEntryStore::removeDir(const std::string& entryName, DirEntry** outDirEntry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr delErr = removeDirUnlocked(entryName, outDirEntry);

   safeLock.unlock();

   return delErr;
}

/**
 * note: only for file dentries
 *
 * @param unlinkEntryName If false do not try to unlink the dentry-name entry, entryName even
 *    might not be set.
 * @param outEntry will be set to the unlinked file and the object must then be deleted by the
 * caller (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr DirEntryStore::unlinkDirEntry(const std::string& entryName, DirEntry* entry,
   unsigned unlinkTypeFlags)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr delErr = unlinkDirEntryUnlocked(entryName, entry, unlinkTypeFlags);

   safeLock.unlock();

   return delErr;
}

/**
 * Note: only for dir dentries
 * Note: This does not lock the mutex, so it must already be locked when calling this.
 *
 * @param outDirEntry  is object of the removed dirEntry, maybe NULL of the caller does not need it
 */
FhgfsOpsErr DirEntryStore::removeDirUnlocked(const std::string& entryName, DirEntry** outDirEntry)
{
   SAFE_ASSIGN(outDirEntry, NULL);

   DirEntry* entry = DirEntry::createFromFile(getDirEntryPathUnlocked(), entryName);
   if(!entry)
      return FhgfsOpsErr_PATHNOTEXISTS;

   if(!DirEntryType_ISDIR(entry->getEntryType() ) )
   {
      delete(entry);
      return FhgfsOpsErr_PATHNOTEXISTS;
   }


   FhgfsOpsErr retVal = DirEntry::removeDirDentry(getDirEntryPathUnlocked(), entryName,
         entry->getIsBuddyMirrored());

   if (outDirEntry)
      *outDirEntry = entry;
   else
      delete(entry);

   return retVal;
}

/**
 * Note: only for file dentries
 * Note: This does not lock the mutex, so it must already be locked when calling this.
 *
 * @param inEntry might be NULL and the entry then needs to be loaded from disk.
 * @param unlinkEntryName If false do not try to unlink the dentry-name entry, entryName even
 *    might not be set. So !unlinkEntryName and !inEntry together are a logical conflict.
 * @param outEntry will be set to the unlinked file and the object must then be deleted by the
 *    caller (can be NULL if the caller is not interested in the file). Also will not be set if
 *    inEntry is not NULL, as the caller already knows the DirEntry then.
 * @param outEntryID for callers that need the entryID and want to avoid the overhead of using
 * outEntry (can be NULL if the caller is not interested); only set on success
 */
FhgfsOpsErr DirEntryStore::unlinkDirEntryUnlocked(const std::string& entryName, DirEntry* entry,
   unsigned unlinkTypeFlags)
{
   FhgfsOpsErr delErr = DirEntry::removeFileDentry(getDirEntryPathUnlocked(), entry->getID(),
      entryName, unlinkTypeFlags, entry->getIsBuddyMirrored());

   return delErr;
}

/**
 * Create a hardlink from 'fromEntryName' to 'toEntryName'.
 *
 * NOTE: Only do this for the entries in the same directory with inlined inodes. In order to avoid
 *       a wrong link count on a possible crash/reboot, the link count already must have been
 *       increased by 1 (If the link count is too high after a crash we might end up with leaked
 *       chunk files on unlink, but not with missing chunk files if done the other way around).
 */
FhgfsOpsErr DirEntryStore::linkEntryInDir(const std::string& fromEntryName,
   const std::string& toEntryName)
{
   const char *logContext = "DirEntryStore renameEntry";

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   std::string fromPath = getDirEntryPathUnlocked() + '/' + fromEntryName;
   std::string toPath   = getDirEntryPathUnlocked() + '/' + toEntryName;

   int linkRes = link(fromPath.c_str(), toPath.c_str() );
   if (linkRes)
   {
      if (errno == EEXIST)
         retVal = FhgfsOpsErr_EXISTS;
      else
      {
         LogContext(logContext).logErr(std::string("Failed to link file in dir: ") +
            getDirEntryPathUnlocked() + " from: " + fromEntryName + " to: " + toEntryName +
            ". SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;
      }
   }

   safeLock.unlock();

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(toPath, MetaSyncFileType::Dentry);

   return retVal;
}

/**
 * In constrast to the moving...()-methods, this method performs a simple rename of an entry,
 * where no moving is involved.
 *
 * @param outRemovedEntry accoring to the rules, this can only be an overwritten file, not a dir
 *
 * NOTE: The caller already must have done rename-sanity checks.
 */
FhgfsOpsErr DirEntryStore::renameEntry(const std::string& fromEntryName,
   const std::string& toEntryName)
{
   const char *logContext = "DirEntryStore renameEntry";

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   std::string fromPath = getDirEntryPathUnlocked() + '/' + fromEntryName;
   std::string toPath   = getDirEntryPathUnlocked() + '/' + toEntryName;

   int renameRes = rename(fromPath.c_str(), toPath.c_str() );

   if (renameRes)
   {
      LogContext(logContext).logErr(std::string("Failed to rename file in dir: ") +
         getDirEntryPathUnlocked() + " from: " + fromEntryName + " to: " + toEntryName +
         ". SysErr: " + System::getErrString() );

      retVal = FhgfsOpsErr_INTERNAL;
   }

   safeLock.unlock();

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         resync->addDeletion(fromPath, MetaSyncFileType::Dentry);
         resync->addModification(toPath, MetaSyncFileType::Dentry);
      }

   return retVal;
}

/**
 * Note: serverOffset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as offset (similar to posix telldir/seekdir).
 *
 * Note: You have reached the end of the directory when success is returned and
 * "outNames.size() != maxOutNames".
 *
 * @param serverOffset zero-based offset; represents the native local fs offset (as in telldir() ).
 * @param filterDots true if "." and ".." should not be returned.
 * @param outArgs outNewOffset is only valid if return value indicates success,
 *    outEntryTypes and outEntryIDs may be NULL, the rest is required.
 */
FhgfsOpsErr DirEntryStore::listIncrementalEx(int64_t serverOffset,
   unsigned maxOutNames, bool filterDots, ListIncExOutArgs& outArgs)
{
   // note: we need offsets here that are stable after unlink, because apps like bonnie++ use
      // readdir(), then unlink() the returned files and expect readdir() to continue after that.
      // this won't work if we use our own offset number and skip the given number of initial
      // entries each time. that's why we use the native local file system offset and seek here.

   const char* logContext = "DirEntryStore (list inc)";

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DIR* dirHandle = opendir(getDirEntryPathUnlocked().c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr(std::string("Unable to open dentry directory: ") +
         getDirEntryPathUnlocked() + ". SysErr: " + System::getErrString() );

      goto err_unlock;
   }


   // seek to offset (if provided)
   if(serverOffset)
   {
      seekdir(dirHandle, serverOffset); // (seekdir has no return value)
   }


   // loop over the actual directory entries
   for( ;
       (numEntries < maxOutNames) &&
          (dirEntry = StorageTk::readdirFilteredEx(dirHandle, filterDots, true) );
       numEntries++)
   {
      outArgs.outNames->push_back(dirEntry->d_name);

      if(outArgs.outServerOffsets)
         outArgs.outServerOffsets->push_back(dirEntry->d_off);

      SAFE_ASSIGN(outArgs.outNewServerOffset, dirEntry->d_off);

      if(outArgs.outEntryTypes || outArgs.outEntryIDs)
      {
         DirEntryType entryType;
         std::string entryID;

         if(!filterDots && !strcmp(dirEntry->d_name, ".") )
         {
            entryType = DirEntryType_DIRECTORY;
            entryID = "<.>";
         }
         else
         if(!filterDots && !strcmp(dirEntry->d_name, "..") )
         {
            entryType = DirEntryType_DIRECTORY;
            entryID = "<..>";
         }
         else
         { // load dentry metadata
            DirEntry entry(dirEntry->d_name);

            bool loadSuccess = entry.loadFromFileName(getDirEntryPathUnlocked(), dirEntry->d_name);
            if (likely(loadSuccess) )
            {
               entryType = entry.getEntryType();
               entryID   = entry.getEntryID();
            }
            else
            { // loading failed
               entryType = DirEntryType_INVALID;
               entryID   = "<invalid>";

               errno = 0;
            }
         }

         if(outArgs.outEntryTypes)
            outArgs.outEntryTypes->push_back( (int)entryType);

         if (outArgs.outEntryIDs)
            outArgs.outEntryIDs->push_back(entryID);
      }

   }

   if(!dirEntry && errno)
   {
      LogContext(logContext).logErr(std::string("Unable to fetch links directory entry from: ") +
         getDirEntryPathUnlocked() + ". SysErr: " + System::getErrString() );
   }
   else
   { // all entries read
      retVal = FhgfsOpsErr_SUCCESS;
   }


   closedir(dirHandle);

err_unlock:
   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Note: serverOffset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 * Note: You have reached the end of the directory when "outNames.size() != maxOutNames".
 * Note: This function was written for fsck
 *
 * @param serverOffset zero-based offset; represents the native local fs offset; preferred over
 * incrementalOffset; use -1 here if you want to seek to e.g. the n-th element and use the slow
 * incrementalOffset for that case.
 * @param incrementalOffset zero-based offset; only used if serverOffset is -1; skips the given
 * number of entries.
 * @param outArgs outNewOffset is only valid if return value indicates success, outEntryTypes is
 * not used (NULL), outNames is required.
 */
FhgfsOpsErr DirEntryStore::listIDFilesIncremental(int64_t serverOffset,
   uint64_t incrementalOffset, unsigned maxOutNames, ListIncExOutArgs& outArgs)
{
   // note: we need offsets here that are stable after unlink, because apps like bonnie++ use
      // readdir(), then unlink() the returned files and expect readdir() to continue after that.
      // this won't work if we use our own offset number and skip the given number of initial
      // entries each time. that's why we use the native local file system offset and seek here.
      // (incrementalOffset is only provided as fallback for client directory seeks and should
      // be avoided when possible.)

   const char* logContext = "DirEntryStore (list ID files inc)";

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   std::string path = MetaStorageTk::getMetaDirEntryIDPath(getDirEntryPathUnlocked());

   DIR* dirHandle = opendir(path.c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr(std::string("Unable to open dentry-by-ID directory: ") +
         path + ". SysErr: " + System::getErrString() );

      goto err_unlock;
   }


   errno = 0; // recommended by posix (readdir(3p) )

   // seek to offset
   if(serverOffset != -1)
   { // caller provided direct offset
      seekdir(dirHandle, serverOffset); // (seekdir has no return value)
   }
   else
   { // slow path: incremental seek to current offset
      for(uint64_t currentOffset = 0;
          (currentOffset < incrementalOffset) && (dirEntry=StorageTk::readdirFiltered(dirHandle) );
          currentOffset++)
      {
         // (actual seek work done in loop header)
         *outArgs.outNewServerOffset = dirEntry->d_off;
      }
   }

   // the actual entry reading
   for( ; (numEntries < maxOutNames) && (dirEntry = StorageTk::readdirFiltered(dirHandle) );
       numEntries++)
   {
      outArgs.outNames->push_back(dirEntry->d_name);
      *outArgs.outNewServerOffset = dirEntry->d_off;
   }

   if(!dirEntry && errno)
   {
      LogContext(logContext).logErr(std::string("Unable to fetch dentry-by-ID entry from: ") +
         path + ". SysErr: " + System::getErrString() );
   }
   else
   { // all entries read
      retVal = FhgfsOpsErr_SUCCESS;
   }


   closedir(dirHandle);

err_unlock:
   safeLock.unlock(); // U N L O C K

   return retVal;
}

bool DirEntryStore::exists(const std::string& entryName)
{
   bool existsRes = false;
   
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K
   
   existsRes = existsUnlocked(entryName);

   safeLock.unlock(); // U N L O C K
   
   return existsRes;   
}

bool DirEntryStore::existsUnlocked(const std::string& entryName)
{
   bool existsRes = false;
   std::string filepath = getDirEntryPathUnlocked() + "/" + entryName;
   struct stat statBuf;

   int statRes = stat(filepath.c_str(), &statBuf);
   if(!statRes)
      existsRes = true;

   return existsRes;
}


/**
 * @param outInodeMetaData might be NULL
 * @return DirEntryType_INVALID if no such dir exists
 *
 */
FhgfsOpsErr DirEntryStore::getEntryData(const std::string& entryName, EntryInfo* outInfo,
   FileInodeStoreData* outInodeMetaData)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;
   
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DirEntry entry(entryName);

   bool loadRes = entry.loadFromFileName(getDirEntryPathUnlocked(), entryName);
   if(loadRes)
   {
      /* copy FileInodeStoreData from entry to outInodeMetaData. We also do not want to allocate
       * a new stripe pattern, so we simply copy the pointer and set
       * entry.dentryInodeMeta.stripePattern = NULL. So on destruction of the object, it will not
       * be deleted.  */
      FileInodeStoreData* inodeDiskData = entry.getInodeStoreData();

      if (outInodeMetaData)
      {
         *outInodeMetaData = *inodeDiskData;
         inodeDiskData->setPattern(NULL);
      }

      int flags = entry.getIsInodeInlined() ? ENTRYINFO_FEATURE_INLINED : 0;

      if (entry.getIsBuddyMirrored())
         flags |= ENTRYINFO_FEATURE_BUDDYMIRRORED;

      NumNodeID ownerNodeID = entry.getOwnerNodeID();
      std::string entryID  = entry.getID();

      outInfo->set(ownerNodeID, this->parentID, entryID, entryName,
         entry.getEntryType(), flags);

      retVal = FhgfsOpsErr_SUCCESS;
   }

   safeLock.unlock(); // U N L O C K
   
   return retVal;
}

/**
 * Load and return the dir-entry of the given entry-/fileName
 */
DirEntry* DirEntryStore::dirEntryCreateFromFile(const std::string& entryName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DirEntry* dirEntry= DirEntry::createFromFile(this->getDirEntryPathUnlocked(), entryName);

   safeLock.unlock(); // U N L O C K

   return dirEntry;
}

/**
 * Note: Unlocked, so make sure you hold the lock when calling this.
 */
const std::string& DirEntryStore::getDirEntryPathUnlocked() const
{
   return dirEntryPath;
}

/**
 * @param entryName
 * @param ownerNode
 *
 * @return FhgfsOpsErr based on the result of the operation
 */
FhgfsOpsErr DirEntryStore::setOwnerNodeID(const std::string& entryName, NumNodeID ownerNode)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   DirEntry entry(entryName);
   bool loadRes;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   loadRes = entry.loadFromFileName(getDirEntryPathUnlocked(), entryName);
   if(!loadRes)
   { // no such entry
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
   }
   else
   {
      if ( entry.setOwnerNodeID(getDirEntryPathUnlocked(), ownerNode) )
      {
         retVal = FhgfsOpsErr_SUCCESS;
      }
   }
   safeLock.unlock();

   return retVal;
}

/**
 * Note: Unlocked. Use this only during init.
 */
void DirEntryStore::setParentID(const std::string& parentID, bool parentIsBuddyMirrored)
{
   this->parentID = parentID;
   this->dirEntryPath = getDirEntryStoreDynamicEntryPath(parentID, parentIsBuddyMirrored);
   this->isBuddyMirrored = parentIsBuddyMirrored;
}
