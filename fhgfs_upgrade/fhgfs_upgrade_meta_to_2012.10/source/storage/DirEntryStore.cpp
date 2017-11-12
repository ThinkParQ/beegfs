#include <common/threading/SafeMutexLock.h>
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
static inline std::string getDirEntryStoreDynamicEntryPath(std::string& parentID)
{
   std::string dentryPath = Program::getApp()->getStructurePath()->getPathAsStrConst();

   return  MetaStorageTk::getMetaDirEntryPath(dentryPath, parentID);
}


/**
 * Note: Sets the parentID to an invalid value, so do not forget to set the parentID before
 * adding any elements.
 */
DirEntryStore::DirEntryStore() :
   parentID("<undef>")
{
}

/**
 * @param parentID ID of the directory to which this store belongs
 */
DirEntryStore::DirEntryStore(std::string parentID) :
   parentID(parentID), dirEntryPath(getDirEntryStoreDynamicEntryPath(parentID) )
{
}

/*
 * Create the new directory for dentries (dir-entries). This directory will contain directory
 * entries of files and sub-directories of the directory given by dirID.
 */
FhgfsOpsErr DirEntryStore::mkDentryStoreDir(std::string dirID)
{
   const char* logContext = "Directory (store initial metadata dir)";

   App* app = Program::getApp();
   std::string contentsDirStr = MetaStorageTk::getMetaDirEntryPath(
      app->getStructurePath()->getPathAsStrConst(), dirID);

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

   return retVal;
}

/**
 * Remove directory for dir-entries (dentries)
 *
 * Note: Assumes that the caller already verified that the directory is empty
 */
bool DirEntryStore::rmDirEntryStoreDir(std::string& id)
{
   const char* logContext = "Directory (remove contents dir)";

   App* app = Program::getApp();
   std::string contentsDirStr = MetaStorageTk::getMetaDirEntryPath(
      app->getStructurePath()->getPathAsStrConst(), id);

   std::string contentsDirIDStr = MetaStorageTk::getMetaDirEntryIDPath(contentsDirStr);

   // remove the dirEntryID directory
   int rmdirIdRes = rmdir(contentsDirIDStr.c_str() );
   if(rmdirIdRes)
   { // error
      LogContext(logContext).logErr("Unable to delete dirEntryID directory: " + contentsDirIDStr  +
         ". " + "SysErr: " + System::getErrString() );

      if (errno != ENOENT)
         return false;
   }

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
   std::string dirEntryPath = this->dirEntryNumIDPath;
   const char* logContext = "make meta dir-entry";

   FhgfsOpsErr mkRes = entry->storeInitialDirEntry(dirEntryPath);

   if (unlikely(mkRes != FhgfsOpsErr_SUCCESS) && mkRes != FhgfsOpsErr_EXISTS)
      LogContext(logContext).logErr(std::string("Failed to create : name: ") + entry->getName() +
         std::string(" entryID: ") + entry->getID() + " in path: " + dirEntryPath);

   return mkRes;
}

/**
 * Create a new dir-entry based on an inode, which is stored in dir-entry format
 * (with inlined inode)
 */
FhgfsOpsErr DirEntryStore::linkInodeToDir(std::string& inodePath, std::string &fileName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr retVal = linkInodeToDirUnlocked(inodePath, fileName);

   safeLock.unlock();

   return retVal;
}


FhgfsOpsErr DirEntryStore::linkInodeToDirUnlocked(std::string& inodePath, std::string &fileName)
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

   return retVal;
}

/**
 * @param outDirEntry is object of the removed dirEntry, maybe NULL of the caller does not need it
 */
FhgfsOpsErr DirEntryStore::removeDir(std::string entryName, DirEntry** outDirEntry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr delErr = removeDirUnlocked(entryName, outDirEntry);

   safeLock.unlock();

   return delErr;
}

/**
 * @param unlinkEntryName If true do not try to unlink the dentry-name entry, entryName even
 *    might not be set.
 * @param outEntry will be set to the unlinked file and the object must then be deleted by the
 * caller (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr DirEntryStore::unlinkDirEntry(std::string entryName, DirEntry* inEntry,
   bool unlinkEntryName, DirEntry** outEntry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr delErr = unlinkDirEntryUnlocked(entryName, inEntry, unlinkEntryName, outEntry);

   safeLock.unlock();

   return delErr;
}

/**
 * Note: This makes sure that the entryName actually refers to the specificied entryType and will
 * return an error otherwise.
 * Note: This does not lock the mutex, so it must already be locked when calling this.
 *
 * @param outDirEntry  is object of the removed dirEntry, maybe NULL of the caller does not need it
 *
 * FIXME Bernd: removeDirUnlocked() and removeFileUnlocked() can be combined into a single function
 */
FhgfsOpsErr DirEntryStore::removeDirUnlocked(std::string entryName, DirEntry** outDirEntry)
{
   if (outDirEntry)
      *outDirEntry = NULL;

   DirEntry* entry = DirEntry::createFromFile(getDirEntryPathUnlocked(), entryName);
   if(!entry)
      return FhgfsOpsErr_PATHNOTEXISTS;

   if(!DirEntryType_ISDIR(entry->getEntryType() ) )
   {
      delete(entry);
      return FhgfsOpsErr_PATHNOTEXISTS;
   }


   FhgfsOpsErr retVal = DirEntry::removeDirDentry(getDirEntryPathUnlocked(), entryName);

   if (outDirEntry)
      *outDirEntry = entry;
   else
      delete(entry);

   return retVal;
}

/**
 * Note: This makes sure that the entryName actually refers to the specificied entryType and will
 * return an error otherwise.
 * Note: This does not lock the mutex, so it must already be locked when calling this.
 *
 * @param inEntry might be NULL and the entry then needs to be loaded from disk.
 * @param unlinkEntryName If true do not try to unlink the dentry-name entry, entryName even
 *    might not be set. So unlinkEntryName == false and !inEntry are a logical conflict!
 * @param outEntry will be set to the unlinked file and the object must then be deleted by the
 *    caller (can be NULL if the caller is not interested in the file). Also will not be set if
 *    inEntry is not NULL, as the caller already knows the DirEntry then.
 */
FhgfsOpsErr DirEntryStore::unlinkDirEntryUnlocked(std::string entryName, DirEntry* inEntry,
   bool unlinkEntryName, DirEntry** outEntry)
{
   const char *logContext = "DirEntryStore unlinkDirEntry";
   DirEntry* entry;

   if (!inEntry)
   {
      #ifdef FHGFS_DEBUG // sanity check
         if (unlikely(!unlinkEntryName) )
         {
            LogContext(logContext).logErr("Bug: inEntry not set and entryName may not be used!");
            LogContext(logContext).logBacktrace();

            return FhgfsOpsErr_INTERNAL;
         }
      #endif

      IGNORE_UNUSED_VARIABLE(logContext);

      entry = DirEntry::createFromFile(getDirEntryPathUnlocked(), entryName);

      if(!entry)
         return FhgfsOpsErr_PATHNOTEXISTS;
   }
   else
      entry = inEntry;

   if(!DirEntryType_ISFILE(entry->getEntryType() ) )
   {
      delete(entry);
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   FhgfsOpsErr delErr = DirEntry::removeFileDentry(getDirEntryPathUnlocked(), entry->getID(),
      entryName, unlinkEntryName);

   if (!inEntry)
   {
      if( (delErr != FhgfsOpsErr_SUCCESS) && outEntry)
      { // persistence error => delete outFile
         delete(*outEntry);
         *outEntry = NULL;
      }

      if(!outEntry)
      { // don't need dir-entry anymore
         delete(entry);
      }
      else
      { // caller needs the object
         *outEntry = entry;
      }
   }

   return delErr;
}

/**
 * In constrast to the moving...()-methods, this method performs a simple rename of an entry,
 * where no moving is involved.
 *
 * @param outRemovedEntry accoring to the rules, this can only be an overwritten file, not a dir
 *
 * NOTE: The caller already must have done rename-sanity checks.
 */
FhgfsOpsErr DirEntryStore::renameEntry(std::string fromEntryName, std::string toEntryName)
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

   return retVal;
}


/**
 * Warning: Returns all names without any limit => can be a very high number => don't use this!
 */
void DirEntryStore::list(StringVector* outNames)
{
   const char* logContext = "DirEntryStore::list";

   errno = 0; // recommended by posix (readdir(3p) )

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DIR* dirHandle = opendir(getDirEntryPathUnlocked().c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr(std::string("Unable to open dentry directory: ") +
         getDirEntryPathUnlocked() + ". SysErr: " + System::getErrString() );
      goto err_unlock;
   }

   struct dirent* dirEntry;

   while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
   {
      outNames->push_back(dirEntry->d_name);
   }

   if(errno)
   {
      LogContext(logContext).logErr(std::string("Unable to fetch dentry directory entry from: ") +
         getDirEntryPathUnlocked() + ". SysErr: " + System::getErrString() );
   }

   closedir(dirHandle);

err_unlock:
   safeLock.unlock(); // U N L O C K
}

/**
 * Note: See listIncrementalEx for comments; this is just a wrapper for it that doesn't retrieve
 * the direntry types.
 */
FhgfsOpsErr DirEntryStore::listIncremental(int64_t serverOffset, uint64_t incrementalOffset,
   unsigned maxOutNames, StringList* outNames, int64_t* outNewServerOffset)
{
   ListIncExOutArgs outArgs = {outNames, NULL, outNewServerOffset};

   return listIncrementalEx(serverOffset, incrementalOffset, maxOutNames, outArgs);
}

/**
 * Note: serverOffset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 * Note: You have reached the end of the directory when "outNames.size() != maxOutNames".
 *
 * @param serverOffset zero-based offset; represents the native local fs offset; preferred over
 * incrementalOffset; use -1 here if you want to seek to e.g. the n-th element and use the slow
 * incrementalOffset for that case.
 * @param incrementalOffset zero-based offset; only used if serverOffset is -1; skips the given
 * number of entries.
 * @param outArgs outNewOffset is only valid if return value indicates success, outEntryTypes can
 * be NULL, outNames is required.
 */
FhgfsOpsErr DirEntryStore::listIncrementalEx(int64_t serverOffset, uint64_t incrementalOffset,
   unsigned maxOutNames, ListIncExOutArgs& outArgs)
{
   // note: we need offsets here that are stable after unlink, because apps like bonnie++ use
      // readdir(), then unlink() the returned files and expect readdir() to continue after that.
      // this won't work if we use our own offset number and skip the given number of initial
      // entries each time. that's why we use the native local file system offset and seek here.
      // (incrementalOffset is only provided as fallback for client directory seeks and should
      // be avoided when possible.)

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

      if(outArgs.outEntryTypes)
      { // load the entry to get the type
         DirEntryType entryType = DirEntry::loadEntryTypeFromFile(
            getDirEntryPathUnlocked(), dirEntry->d_name);
         outArgs.outEntryTypes->push_back( (int)entryType);

         if(unlikely(!DirEntryType_ISVALID(entryType) ) )
            errno = 0; // reset errno for "!dirEntry && errno" check below
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

bool DirEntryStore::exists(std::string entryName)
{
   bool existsRes = false;
   
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K
   
   existsRes = existsUnlocked(entryName);

   safeLock.unlock(); // U N L O C K
   
   return existsRes;   
}

bool DirEntryStore::existsUnlocked(std::string entryName)
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
 * @param outInfo depth will not be set
 * @return DirEntryType_INVALID if no such dir exists
 *
 */
DirEntryType DirEntryStore::getEntryInfo(std::string entryName, EntryInfo* outInfo,
   DentryInodeMeta* outInodeMetaData)
{
   DirEntryType retVal = DirEntryType_INVALID;
   
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DirEntry entry(entryName);

   bool loadRes = entry.loadFromFileName(getDirEntryPathUnlocked(), entryName);
   if(loadRes)
   {
      /* copy DentryInodeMeta from entry to outInodeMetaData. We also do not want to allocate
       * a new stripe pattern, so we simply copy the pointer and set
       * entry.dentryInodeMeta.stripePattern = NULL. So on destruction of the object, it will not
       * be deleted.  */
      DentryInodeMeta* inodeMetaData = entry.getDentryInodeMeta();
      *outInodeMetaData = *inodeMetaData;
      inodeMetaData->setPattern(NULL);

      int flags = outInodeMetaData->getDentryFeatureFlags() & DISKMETADATA_FEATURE_INODE_INLINE ?
         ENTRYINFO_FLAG_INLINED : 0;

      uint16_t ownerNodeID = entry.getOwnerNodeID();
      std::string entryID  = entry.getID();

      outInfo->update(ownerNodeID, this->parentID, entryID, entryName,
         entry.getEntryType(), flags);

      retVal = entry.getEntryType();
   }

   safeLock.unlock(); // U N L O C K
   
   return retVal;
}

/**
 * Load and return the dir-entry of the given entry-/fileName
 */
DirEntry* DirEntryStore::dirEntryCreateFromFile(std::string entryName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DirEntry* dirEntry= DirEntry::createFromFile(this->getDirEntryPathUnlocked(), entryName);

   safeLock.unlock(); // U N L O C K

   return dirEntry;
}

/**
 * Note: Unlocked, so make sure you hold the lock when calling this.
 */
const std::string& DirEntryStore::getDirEntryPathUnlocked()
{
   return dirEntryPath;
}

/**
 * @param entryName
 * @param ownerNode
 *
 * @return FhgfsOpsErr based on the result of the operation
 */
FhgfsOpsErr DirEntryStore::setOwnerNodeID(std::string entryName, uint16_t ownerNode)
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
void DirEntryStore::setParentID(std::string parentID, std::string updatedParentID)
{
   this->parentID = parentID;

   this->dirEntryPath = getDirEntryStoreDynamicEntryPath(parentID);

   this->dirEntryNumIDPath = getDirEntryStoreDynamicEntryPath(updatedParentID);
}

/**
 * Only unlink a dir-entry-name entry, but not its dir-entry-id hard-link. We may use that for
 * example for rename(), where we overwrite an entry. But if creation of the new entry should
 * fail, we can still use the ID-backup file to re-create the dir-entry-name entry.
 */
FhgfsOpsErr DirEntryStore::unlinkDirEntryName(std::string entryName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr retVal = unlinkDirEntryNameUnlocked(entryName);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

FhgfsOpsErr DirEntryStore::unlinkDirEntryNameUnlocked(std::string entryName)
{
   std::string filepath = getDirEntryPathUnlocked() + "/" + entryName;

   FhgfsOpsErr retVal;

   int unlinkRes = unlink(filepath.c_str() );
   if(unlinkRes)
   {
      if (errno == ENOENT)
         retVal  = FhgfsOpsErr_PATHNOTEXISTS;
      else
         retVal = FhgfsOpsErr_INTERNAL;
   }
   else
      retVal = FhgfsOpsErr_SUCCESS;

   return retVal;
}

