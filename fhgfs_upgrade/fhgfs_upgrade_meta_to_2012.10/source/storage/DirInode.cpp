#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/Raid10Pattern.h>
#include <common/toolkit/TimeAbs.h>
#include <program/Program.h>

#include <sys/types.h>
#include <attr/xattr.h>
#include <dirent.h>

#include "DiskMetaData.h"
#include "DirInode.h"

DirInode::DirInode(std::string id, int mode, unsigned userID, unsigned groupID,
   uint16_t ownerNodeID, StripePattern& stripePattern) : id(id), ownerNodeID(ownerNodeID),
   entries(id)
{
   //this->id = id; // done in initializer list
   //this->ownerNodeID = ownerNodeID; // done in initializer list
   this->stripePattern      = stripePattern.clone();
   this->parentNodeID       = 0; // 0 means undefined
   this->featureFlags       = 0;
   this->exclusive          = false;
   int64_t currentTimeSecs  = TimeAbs().getTimeval()->tv_sec;
   int64_t fileSize         = 0; // irrelevant for dirs, but numSubdirs + numFiles in stat calls
   unsigned nlink           = 2; // "." and ".." for now only
   this->numSubdirs         = 0;
   this->numFiles           = 0;
   unsigned contentsVersion = 0;

   SettableFileAttribs settableAttribs = { mode, userID, groupID, currentTimeSecs,currentTimeSecs };

   this->statData.setAttribChangeTimeSecs(currentTimeSecs);
   this->statData.setModificationTimeSecs(currentTimeSecs);
   this->statData.setLastAccessTimeSecs(currentTimeSecs);

   this->statData.set(fileSize, &settableAttribs, currentTimeSecs, currentTimeSecs, nlink,
      contentsVersion);

   this->isLoaded = true;
}

/**
 * Create a stripe pattern based on the current pattern config and add targets to it.
 *
 * @param preferredTargets may be NULL
 */
StripePattern* DirInode::createFileStripePattern(UInt16List* preferredTargets)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   StripePattern* filePattern = createFileStripePatternUnlocked(preferredTargets);

   safeLock.unlock(); // U N L O C K

   return filePattern;
}

StripePattern* DirInode::createFileStripePatternUnlocked(UInt16List* preferredTargets)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   TargetCapacityPools* capacityPools = app->getStorageCapacityPools();
   TargetChooserType chooserType = cfg->getTuneTargetChooserNum();

   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return NULL;

   StripePattern* filePattern;

   // choose some available storage targets

   unsigned desiredNumTargets = stripePattern->getDefaultNumTargets();
   UInt16Vector stripeTargets;

   if(chooserType == TargetChooserType_RANDOMIZED)
   { // randomized chooser
      capacityPools->chooseStorageTargets(
         desiredNumTargets, preferredTargets, &stripeTargets);
   }
   else
   { // round robin or randomized round robin chooser
      capacityPools->chooseStorageTargetsRoundRobin(
         desiredNumTargets, &stripeTargets);

      if(chooserType == TargetChooserType_RANDOMROBIN)
         random_shuffle(stripeTargets.begin(), stripeTargets.end() ); // randomize result vector
   }


   // check whether we got enough targets...

   if(unlikely(stripeTargets.size() < stripePattern->getMinNumTargets() ) )
      return NULL;

   // clone the dir pattern with new stripeTargets...

   if(cfg->getTuneRotateMirrorTargets() &&
      (stripePattern->getPatternType() == STRIPEPATTERN_Raid10) )
      filePattern = ( (Raid10Pattern*)stripePattern)->cloneRotated(stripeTargets);
   else
      filePattern = stripePattern->clone(stripeTargets);

   return filePattern;
}

StripePattern* DirInode::getStripePatternClone()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

   StripePattern* pattern = stripePattern->clone();

   safeLock.unlock();

   return pattern;
}

/**
 * @param newPattern will be cloned
 */
bool DirInode::setStripePattern(StripePattern& newPattern)
{
   bool success = true;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
   {
      safeLock.unlock();
      return loadSuccess;
   }

   StripePattern* oldPattern = this->stripePattern;
   this->stripePattern = newPattern.clone();

   if(!storeUpdatedMetaData() )
   { // failed to update metadata => restore old value
      delete(this->stripePattern); // delete newPattern-clone
      this->stripePattern = oldPattern;

      success = false;
   }
   else
   { // sucess
      delete(oldPattern);
   }

   safeLock.unlock();

   return success;
}

/**
 * Note: See listIncrementalEx for comments; this is just a wrapper for it that doen't retrieve the
 * direntry types.
 */
FhgfsOpsErr DirInode::listIncremental(int64_t serverOffset, uint64_t incrementalOffset,
   unsigned maxOutNames, StringList* outNames, int64_t* outNewServerOffset)
{
   ListIncExOutArgs outArgs = {outNames, NULL, outNewServerOffset};

   return listIncrementalEx(serverOffset, incrementalOffset, maxOutNames, outArgs);
}

/**
 * Note: Returns only a limited number of entries.
 *
 * Note: serverOffset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 *
 * @param serverOffset zero-based offset
 * @param incrementalOffset zero-based offset; only used if serverOffset is -1
 * @param maxOutNames the maximum number of entries that will be added to the outNames
 * @param outArgs outNewOffset is only valid if return value indicates success, outEntryTypes can
 * be NULL, outNames is required.
 */
FhgfsOpsErr DirInode::listIncrementalEx(int64_t serverOffset, uint64_t incrementalOffset,
   unsigned maxOutNames, ListIncExOutArgs& outArgs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr listRes = entries.listIncrementalEx(serverOffset, incrementalOffset, maxOutNames,
      outArgs);

   safeLock.unlock(); // U N L O C K

   return listRes;
}

/**
 * Note: Returns only a limited number of dentry-by-ID file names
 *
 * Note: serverOffset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 *
 * Note: this function was written for fsck
 *
 * @param serverOffset zero-based offset
 * @param incrementalOffset zero-based offset; only used if serverOffset is -1
 * @param maxOutNames the maximum number of entries that will be added to the outNames
 * @param outArgs outNewOffset is only valid if return value indicates success, outEntryTypes is
 * not used (NULL), outNames is required.
 */
FhgfsOpsErr DirInode::listIDFilesIncremental(int64_t serverOffset, uint64_t incrementalOffset,
   unsigned maxOutNames, ListIncExOutArgs& outArgs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr listRes = entries.listIDFilesIncremental(serverOffset, incrementalOffset,
      maxOutNames, outArgs);

   safeLock.unlock(); // U N L O C K

   return listRes;
}

/**
 * Check whether an entry with the given name exists in this directory.
 */
bool DirInode::exists(std::string entryName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   bool retVal = entries.exists(entryName);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * @param outInfo outInfo->depth will not be set
 * @return DirEntryType_INVALID if entry does not exist
 */
DirEntryType DirInode::getEntryInfo(std::string entryName, EntryInfo* outInfo,
   DentryInodeMeta* outInodeMetaData)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DirEntryType retVal = entries.getEntryInfo(entryName, outInfo, outInodeMetaData);

   safeLock.unlock(); // U N L O C K

   return retVal;
}


/**
 * Create new directory entry in the directory given by DirInode
 *
 * Note: Do not use or delete entry after calling this method
 */
FhgfsOpsErr DirInode::makeDirEntry(DirEntry* entry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // we always delete the entry from this method
   FhgfsOpsErr mkRes = makeDirEntryUnlocked(entry, true);

   safeLock.unlock(); // U N L O C K

   return mkRes;
}

/**
 * @param deleteEntry  shall we delete entry or does the caller still need it?
 */
FhgfsOpsErr DirInode::makeDirEntryUnlocked(DirEntry* entry, bool deleteEntry)
{
   FhgfsOpsErr mkRes = FhgfsOpsErr_INTERNAL;

   DirEntryType entryType = entry->getEntryType();
   if (unlikely( (!DirEntryType_ISFILE(entryType) && (!DirEntryType_ISDIR(entryType) ) ) ) )
      goto out;

   // load DirInode on demand if required, we need it now
   if (loadIfNotLoadedUnlocked() == false)
   {
      mkRes = FhgfsOpsErr_PATHNOTEXISTS;
      goto out;
   }

   mkRes = this->entries.makeEntry(entry);
   if(mkRes == FhgfsOpsErr_SUCCESS)
   {
      // entry successfully created, update our own dirInode
      if (DirEntryType_ISDIR(entryType) )
         increaseNumSubDirsAndStoreOnDisk();
      else
         increaseNumFilesAndStoreOnDisk();
   }

out:
   if (deleteEntry)
      delete entry;

   return mkRes;
}

/**
 * Link an existing inode (stored in dir-entry format) to our directory. So add a new dirEntry.
 *
 * Used for example for linking an (unlinked/deleted) file-inode to the disposal dir.
 */
FhgfsOpsErr DirInode::linkFileInodeToDir(std::string& inodePath, std::string &fileName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // we always delete the entry from this method
   FhgfsOpsErr retVal = linkFileInodeToDirUnlocked(inodePath, fileName);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

FhgfsOpsErr DirInode::linkFileInodeToDirUnlocked(std::string& inodePath, std::string &fileName)
{
   bool loadRes = loadIfNotLoadedUnlocked();
   if (!loadRes)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr retVal = this->entries.linkInodeToDir(inodePath, fileName);

   if (retVal == FhgfsOpsErr_SUCCESS)
      increaseNumFilesAndStoreOnDisk();

   return retVal;
}

FhgfsOpsErr DirInode::removeDir(std::string entryName, DirEntry** outDirEntry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr rmRes = removeDirUnlocked(entryName, outDirEntry);

   safeLock.unlock(); // U N L O C K

   return rmRes;
}

/**
 * @param outDirEntry is object of the removed dirEntry, maybe NULL of the caller does not need it
 */
FhgfsOpsErr DirInode::removeDirUnlocked(std::string entryName, DirEntry** outDirEntry)
{
   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr rmRes = entries.removeDir(entryName, outDirEntry);

   if(rmRes == FhgfsOpsErr_SUCCESS)
   {
      if (unlikely (!decreaseNumSubDirsAndStoreOnDisk() ) )
         rmRes = FhgfsOpsErr_INTERNAL;
   }

   return rmRes;
}
/**
 *
 * @param unlinkEntryName If true do not try to unlink the dentry-name entry, entryName even
 *    might not be set.
 * @param outFile will be set to the unlinked file and the object must then be deleted by the caller
 * (can be NULL if the caller is not interested in the file)
 */

FhgfsOpsErr DirInode::renameDirEntry(std::string fromName, std::string toName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr renameRes = renameDirEntryUnlocked(fromName, toName);

   safeLock.unlock(); // U N L O C K

   return renameRes;
}

FhgfsOpsErr DirInode::renameDirEntryUnlocked(std::string fromName, std::string toName)
{
   const char* logContext = "DirInode rename entry";

   FhgfsOpsErr renameRes = entries.renameEntry(fromName, toName);

   if(renameRes == FhgfsOpsErr_SUCCESS)
   {
      // ignore the return code, time stamps are not that important
      updateTimeStampsAndStoreToDisk(logContext);
   }

   return renameRes;
}

/**
 *
 * @param unlinkEntryName If true do not try to unlink the dentry-name entry, entryName even
 *    might not be set.
 * @param outFile will be set to the unlinked file and the object must then be deleted by the caller
 * (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr DirInode::unlinkDirEntry(std::string entryName, DirEntry* inEntry,
   bool unlinkEntryName, DirEntry** outEntry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr unlinkRes = unlinkDirEntryUnlocked(entryName, inEntry, unlinkEntryName, outEntry);

   safeLock.unlock(); // U N L O C K

   return unlinkRes;
}

FhgfsOpsErr DirInode::unlinkDirEntryUnlocked(std::string entryName, DirEntry* inEntry,
   bool unlinkEntryName, DirEntry** outEntry)
{
   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr unlinkRes = entries.unlinkDirEntry(entryName, inEntry, unlinkEntryName, outEntry);

   if(unlinkRes == FhgfsOpsErr_SUCCESS)
   {
      if(unlikely(!decreaseNumFilesAndStoreOnDisk() ) )
         unlinkRes = FhgfsOpsErr_INTERNAL;
   }

   return unlinkRes;
}


/**
 * Re-computes number of subentries.
 *
 * Note: Intended to be called only when you have reason to believe that the stored metadata is
 * not correct (eg after an upgrade that introduced new metadata information).
 */
FhgfsOpsErr DirInode::refreshMetaInfo()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr retVal = refreshSubentryCountUnlocked();
   if(retVal == FhgfsOpsErr_SUCCESS)
   {
      if(!storeUpdatedMetaData() )
         retVal = FhgfsOpsErr_INTERNAL;
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Reads all stored entry links (via entries::listInc() ) to re-compute number of subentries.
 *
 * Note: Intended to be called only when you have reason to believe that the stored metadata is
 * not correct (eg after an upgrade that introduced new metadata information).
 * Note: Does not update persistent metadata.
 * Note: Unlocked, so hold the write lock when calling this.
 */
FhgfsOpsErr DirInode::refreshSubentryCountUnlocked()
{
   const char* logContext = "Directory (refresh entry count)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   unsigned currentNumSubdirs = 0;
   unsigned currentNumFiles = 0;

   unsigned maxOutNames = 128;
   StringList names;
   IntList entryTypes;
   int64_t serverOffset = 0;
   size_t numOutEntries; // to avoid inefficient calls to list::size.

   // query contents
   ListIncExOutArgs outArgs = { &names, &entryTypes, &serverOffset };

   do
   {
      names.clear();
      entryTypes.clear();

      FhgfsOpsErr listRes = entries.listIncrementalEx(serverOffset, 0, maxOutNames, outArgs);
      if(listRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).logErr(
            std::string("Unable to fetch dir contents for dirID: ") + id);
         retVal = listRes;
         break;
      }

      // walk over returned entries

      StringListIter namesIter = names.begin();
      IntListIter typesIter = entryTypes.begin();
      numOutEntries = 0;

      for( /* using namesIter, typesIter, numOutEntries */;
          (namesIter != names.end() ) && (typesIter != entryTypes.end() );
          namesIter++, typesIter++, numOutEntries++)
      {
         if(DirEntryType_ISDIR(*typesIter) )
            currentNumSubdirs++;
         else
         if(DirEntryType_ISFILE(*typesIter) )
            currentNumFiles++;
         else
         { // invalid entry => log error, but continue
            LogContext(logContext).logErr(std::string("Unable to read entry type for name: '") +
               *namesIter + "' "  "(dirID: " + id + ")");
         }
      }

   } while(numOutEntries == maxOutNames);

   if(retVal == FhgfsOpsErr_SUCCESS)
   { // update in-memory counters (but we leave persistent updates to the caller)
      this->numSubdirs = currentNumSubdirs;
      this->numFiles = currentNumFiles;
   }

   return retVal;
}


/*
 * Note: Current object state is used for the serialization.
 */
FhgfsOpsErr DirInode::storeInitialMetaData()
{
   FhgfsOpsErr dirRes = DirEntryStore::mkDentryStoreDir(this->updatedID);
   if(dirRes != FhgfsOpsErr_SUCCESS)
      return dirRes;

   FhgfsOpsErr fileRes = storeInitialMetaDataInode(updatedID);
   if(unlikely(fileRes != FhgfsOpsErr_SUCCESS) )
   {
      if (unlikely(fileRes == FhgfsOpsErr_EXISTS) )
      {
         // there must have been some kind of race as dirRes was successful
         fileRes = FhgfsOpsErr_SUCCESS;
      }
      else
         DirEntryStore::rmDirEntryStoreDir(updatedID); // remove dir
   }

   return fileRes;
}

/*
 * Creates the initial metadata inode for this directory.
 *
 * Note: Current object state is used for the serialization.
 */
FhgfsOpsErr DirInode::storeInitialMetaDataInode(std::string entryID)
{
   const char* logContext = "Directory (store initial metadata file)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), entryID);

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   bool useXAttrs = app->getConfig()->getStoreUseExtendedAttribs();

   char* buf;
   unsigned bufLen;

   // create file

   int openFlags = O_CREAT|O_WRONLY;

   int fd = open(metaFilename.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      if(errno == EEXIST)
         retVal = FhgfsOpsErr_EXISTS;
      else
      {
         LogContext(logContext).logErr("Unable to create dir metadata inode " + metaFilename +
            ". " + "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;
      }

      goto error_donothing;
   }

   // alloc buf and serialize

   buf = (char*)malloc(META_SERBUF_SIZE);
   bufLen = DiskMetaData::serializeDirInode(buf, this);

   // write data to file

   if(useXAttrs)
   { // extended attribute
      int setRes = fsetxattr(fd, META_XATTR_DIR_NAME, buf, bufLen, 0);
      free(buf);

      if(unlikely(setRes == -1) )
      { // error
         if(errno == ENOTSUP)
            LogContext(logContext).logErr("Unable to store directory xattr metadata: " +
               metaFilename + ". " +
               "Did you enable extended attributes (user_xattr) on the underlying file system?");
         else
            LogContext(logContext).logErr("Unable to store directory xattr metadata: " +
               metaFilename + ". " + "SysErr: " + System::getErrString() );

         retVal = FhgfsOpsErr_INTERNAL;

         goto error_closefile;
      }
   }
   else
   { // normal file content
      ssize_t writeRes = write(fd, buf, bufLen);
      free(buf);

      if(writeRes != (ssize_t)bufLen)
      {
         LogContext(logContext).logErr("Unable to store directory metadata: " + metaFilename +
            ". " + "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;

         goto error_closefile;
      }
   }

   close(fd);

   LOG_DEBUG(logContext, 4, "Directory metadata inode stored: " + metaFilename);

   return retVal;


   // error compensation
error_closefile:
   close(fd);
   unlink(metaFilename.c_str() );

error_donothing:

   return retVal;
}

/**
 * Note: Wrapper/chooser for storeUpdatedMetaDataBufAsXAttr/Contents.
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBuf(char* buf, unsigned bufLen)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return storeUpdatedMetaDataBufAsXAttr(buf, bufLen);

   return storeUpdatedMetaDataBufAsContents(buf, bufLen);
}

/**
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen)
{
   const char* logContext = "Directory (store updated xattr metadata)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), this->updatedID);

   // write data to file

   int setRes = setxattr(metaFilename.c_str(), META_XATTR_DIR_NAME, buf, bufLen, 0);

   if(unlikely(setRes == -1) )
   { // error
      LogContext(logContext).logErr("Unable to write directory metadata update: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   LOG_DEBUG(logContext, 4, "Directory metadata update stored: " + id);

   return true;
}

/**
 * Stores the update to a sparate file first and then renames it.
 *
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen)
{
   const char* logContext = "Directory (store updated metadata)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), this->updatedID);
   std::string metaUpdateFilename(metaFilename + META_UPDATE_EXT_STR);

   ssize_t writeRes;
   int renameRes;

   // open file (create it, but not O_EXCL because a former update could have failed)
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(metaUpdateFilename.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      if(errno == ENOSPC)
      { // no free space => try again with update in-place
         LogContext(logContext).log(Log_DEBUG, "No space left to create update file. Trying update "
            "in-place: " + metaUpdateFilename + ". " + "SysErr: " + System::getErrString() );

         return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen);
      }

      LogContext(logContext).logErr("Unable to create directory metadata update file: " +
         metaUpdateFilename + ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   // metafile created => store meta data
   writeRes = write(fd, buf, bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      if( (writeRes >= 0) || (errno == ENOSPC) )
      { // no free space => try again with update in-place
         LogContext(logContext).log(Log_DEBUG, "No space left to write update file. Trying update "
            "in-place: " + metaUpdateFilename + ". " + "SysErr: " + System::getErrString() );

         close(fd);
         unlink(metaUpdateFilename.c_str() );

         return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen);
      }

      LogContext(logContext).logErr("Unable to store directory metadata update: " + metaFilename +
         ". " + "SysErr: " + System::getErrString() );

      goto error_closefile;
   }

   close(fd);

   renameRes = rename(metaUpdateFilename.c_str(), metaFilename.c_str() );
   if(renameRes == -1)
   {
      LogContext(logContext).logErr("Unable to replace old directory metadata file: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      goto error_unlink;
   }

   LOG_DEBUG(logContext, 4, "Directory metadata update stored: " + id);

   return true;


   // error compensation
error_closefile:
   close(fd);

error_unlink:
   unlink(metaUpdateFilename.c_str() );

   return false;
}

/**
 * Stores the update directly to the current metadata file (instead of creating a separate file
 * first and renaming it).
 *
 * Note: Don't call this directly, it is automatically called by storeUpdatedMetaDataBufAsContents()
 * when necessary.
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen)
{
   const char* logContext = "Directory (store updated metadata in-place)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), this->updatedID);

   int fallocRes;
   ssize_t writeRes;
   int truncRes;

   // open file (create it, but not O_EXCL because a former update could have failed)
   int openFlags = O_CREAT|O_WRONLY;

   int fd = open(metaFilename.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      LogContext(logContext).logErr("Unable to open directory metadata file: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   // make sure we have enough room to write our update
   fallocRes = posix_fallocate(fd, 0, bufLen); // (note: posix_fallocate does not set errno)
   if(fallocRes == EBADF)
   { // special case for XFS bug
      struct stat statBuf;
      int statRes = fstat(fd, &statBuf);

      if (statRes == -1)
      {
         LogContext(logContext).log(Log_WARNING, "Unexpected error: fstat() failed with SysErr: "
            + System::getErrString(errno));
         goto error_closefile;
      }

      if (statBuf.st_size < bufLen)
      {
         LogContext(logContext).log(Log_WARNING, "File space allocation ("
            + StringTk::intToStr(bufLen)  + ") for metadata update failed: " + metaFilename + ". " +
            "SysErr: " + System::getErrString(fallocRes) + " "
            "statRes: " + StringTk::intToStr(statRes) + " "
            "oldSize: " + StringTk::intToStr(statBuf.st_size));
         goto error_closefile;
      }
      else
      { // // XFS bug! We only return an error if statBuf.st_size < bufLen. Ingore fallocRes then
         LOG_DEBUG(logContext, Log_DEBUG, "Kernel file system bug, posix_fallocate() failed "
            "for len < filesize, but should not!");
      }
   }
   else
   if (fallocRes != 0)
   { // default error handling if posix_fallocate() failed
      LogContext(logContext).log(Log_WARNING, "File space allocation ("
         + StringTk::intToStr(bufLen)  + ") for metadata update failed: " +  metaFilename + ". " +
         "SysErr: " + System::getErrString(fallocRes));
      goto error_closefile;
   }

   // metafile created => store meta data
   writeRes = write(fd, buf, bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      LogContext(logContext).logErr("Unable to store directory metadata update: " + metaFilename +
         ". " + "SysErr: " + System::getErrString() );

      goto error_closefile;
   }

   // truncate in case the update lead to a smaller file size
   truncRes = ftruncate(fd, bufLen);
   if(truncRes == -1)
   { // ignore trunc errors
      LogContext(logContext).log(Log_WARNING, "Unable to truncate metadata file (strange, but "
         "proceeding anyways): " + metaFilename + ". " + "SysErr: " + System::getErrString() );
   }


   close(fd);

   LOG_DEBUG(logContext, 4, "Directory metadata in-place update stored: " + id);

   return true;


   // error compensation
error_closefile:
   close(fd);

   return false;
}


bool DirInode::storeUpdatedMetaData()
{
   std::string logContext = "Write Meta Inode";

   if (unlikely(!this->isLoaded) )
   {
      LogContext(logContext).logErr("Bug: Inode data not loaded yet.");
      LogContext(logContext).logBacktrace();

      return false;
   }

#if 0
   #ifdef DEBUG_MUTEX_LOCKING
      if (unlikely(!this->rwlock.isRWLocked() ) )
      {
         LogContext(logContext).logErr("Bug: Inode not rw-locked.");
         LogContext(logContext).logBacktrace();

         return false;
      }
   #endif
#endif

   char* buf = (char*)malloc(META_SERBUF_SIZE);

   unsigned bufLen = DiskMetaData::serializeDirInode(buf, this);

   bool saveRes = storeUpdatedMetaDataBuf(buf, bufLen);

   free(buf);

   return saveRes;
}

/**
 * Note: Assumes that the caller already verified that the directory is empty
 */
bool DirInode::removeStoredMetaData(std::string id)
{
   bool dirRes = DirEntryStore::rmDirEntryStoreDir(id);
   if(!dirRes)
      return dirRes;

   return removeStoredMetaDataFile(id);
}


/**
 * Note: Assumes that the caller already verified that the directory is empty
 */
bool DirInode::removeStoredMetaDataFile(std::string& id)
{
   const char* logContext = "Directory (remove stored metadata)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), id);

   // delete metadata file

   int unlinkRes = unlink(metaFilename.c_str() );
   if(unlinkRes == -1)
   { // error
      LogContext(logContext).logErr("Unable to delete directory metadata file: " + metaFilename +
         ". " + "SysErr: " + System::getErrString() );

      if (errno != ENOENT)
         return false;
   }

   LOG_DEBUG(logContext, 4, "Directory metadata deleted: " + metaFilename);

   return true;
}

bool DirInode::loadIfNotLoaded(void)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   bool loadRes = loadIfNotLoadedUnlocked();

   safeLock.unlock();

   return loadRes;
}


/**
 * Load the DirInode from disk if it was notalready loaded before.
 *
 * @return true if loading not required or loading successfully, false if loading from disk failed.
 */
bool DirInode::loadIfNotLoadedUnlocked()
{
   if (!this->isLoaded)
   { // So the object was already created before without loading the inode from disk, do that now.
      bool loadSuccess = loadFromFile();
      if (!loadSuccess)
      {
         const char* logContext = "Loading DirInode on demand";
         std::string msg = "Loading DirInode failed dir-ID: ";
         LOG_DEBUG_CONTEXT(LogContext(logContext), Log_DEBUG, msg + this->id);
         IGNORE_UNUSED_VARIABLE(logContext);

         return false;
      }
   }

   return true;
}

/**
 * Note: Wrapper/chooser for loadFromFileXAttr/Contents.
 */
bool DirInode::loadFromFile()
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   bool loadRes;
   if(useXAttrs)
      loadRes = loadFromFileXAttr();
   else
      loadRes = loadFromFileContents();

   if (loadRes)
      this->isLoaded = true;

   return loadRes;
}


/**
 * Note: Don't call this directly, use the wrapper loadFromFile().
 */
bool DirInode::loadFromFileXAttr()
{
   const char* logContext = "Directory (load from xattr file)";

   App* app = Program::getApp();

   this->oldHashDirPath = StorageTkEx::getOldMetaInodePath("", id + META_SUBDIR_SUFFIX_STR_OLD);
   std::string inodePath = app->getOldInodesPath()->getPathAsStrConst() + this->oldHashDirPath;
   this->pathToDirInode = inodePath;

   bool retVal = false;

   char* buf = (char*)malloc(META_SERBUF_SIZE);

   ssize_t getRes = getxattr(inodePath.c_str(), META_XATTR_DIRINODE_NAME_OLD, buf, META_SERBUF_SIZE);
   if(getRes > 0)
   { // we got something => deserialize it
      bool deserRes = DiskMetaData::deserializeDirInode(buf, this);
      if(unlikely(!deserRes) )
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize metadata in file: " + inodePath);
         goto error_freebuf;
      }

      retVal = true;
   }
   else
   { // unhandled error
      LogContext(logContext).logErr("Unable to open/read xattr inode file: " +
         inodePath + ". " + "SysErr: " + System::getErrString() );
   }


error_freebuf:
   free(buf);

   return retVal;
}

/**
 * Note: Don't call this directly, use the wrapper loadFromFile().
 */
bool DirInode::loadFromFileContents()
{
   const char* logContext = "Directory (load from file)";

   App* app = Program::getApp();

   this->oldHashDirPath = StorageTkEx::getOldMetaInodePath("", id + META_SUBDIR_SUFFIX_STR_OLD);
   std::string inodePath = app->getOldInodesPath()->getPathAsStrConst() + this->oldHashDirPath;
   this->pathToDirInode = inodePath;

   bool retVal = false;

   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(inodePath.c_str(), openFlags, 0);
   if(fd == -1)
   { // open failed
      LogContext(logContext).logErr("Unable to open inode file: " + inodePath +
         ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   char* buf = (char*)malloc(META_SERBUF_SIZE);
   int readRes = read(fd, buf, META_SERBUF_SIZE);
   if(readRes <= 0)
   { // reading failed
      LogContext(logContext).logErr("Unable to read inode file: " + inodePath + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   {
      bool deserRes = DiskMetaData::deserializeDirInode(buf, this);
      if(!deserRes)
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize metadata in file: " + inodePath);
      }
      else
      { // success
         retVal = true;
      }
   }

   free(buf);
   close(fd);

   return retVal;
}


DirInode* DirInode::createFromFile(std::string id)
{
   DirInode* newDir = new DirInode(id);

   if (!newDir->initializedUpdatedID)
   {
      delete(newDir);
      return NULL;
   }

   bool loadRes = newDir->loadFromFile();
   if(!loadRes)
   {
      delete(newDir);
      return NULL;
   }

   if (!Upgrade::updateEntryID(newDir->parentDirID, newDir->updatedParentDirID) )
   {
      delete(newDir);
      return NULL;
   }

   newDir->entries.setParentID(id, newDir->updatedID);

   return newDir;
}

FhgfsOpsErr DirInode::getStatData(StatData& outStatData)
{
   // note: keep in mind that correct nlink count (2+numSubdirs) is absolutely required to not break
      // the "find" command, which uses optimizations based on nlink (see "find -noleaf" for
      // details). The other choice would be to set nlink=1, which find would reduce to -1
      // as is substracts 2 for "." and "..". -1 = max(unsigned) and makes find to check all
      // possible subdirs. But as we have the correct nlink anyway, we can provide it to find
      // and allow it to use non-posix optimizations.

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

   this->statData.setNumHardLinks(2 + numSubdirs); // +2 by definition (for "." and "<name>")

   this->statData.setFileSize(numSubdirs + numFiles); // just because we got those values anyway

   outStatData = this->statData;

   safeLock.unlock();

   return FhgfsOpsErr_SUCCESS;
}

/*
 * only used for testing at the moment
 */
void DirInode::setStatData(StatData& statData)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   this->statData = statData;

   this->numSubdirs = statData.getNumHardlinks() - 2;
   this->numFiles = statData.getFileSize() - this->numSubdirs;

   storeUpdatedMetaData();

   safeLock.unlock();
}

/**
 * @param validAttribs SETATTR_CHANGE_...-Flags, might be 0 if only attribChangeTimeSecs
 *    shall be updated
 * @attribs  new attributes, but might be NULL if validAttribs == 0
 */
bool DirInode::setAttrData(int validAttribs, SettableFileAttribs* attribs)
{
   bool success = true;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // save old attribs
   SettableFileAttribs oldAttribs;
   oldAttribs = *(this->statData.getSettableFileAttribs() );

    this->statData.setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);

   if(validAttribs)
   { // apply new attribs wrt flags...

      if(validAttribs & SETATTR_CHANGE_MODE)
         this->statData.setMode(attribs->mode);

      if(validAttribs & SETATTR_CHANGE_MODIFICATIONTIME)
      {
         /* only static value update required for storeUpdatedMetaData() */
         this->statData.setModificationTimeSecs(attribs->modificationTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_LASTACCESSTIME)
      {
         /* only static value update required for storeUpdatedMetaData() */
         this->statData.setLastAccessTimeSecs(attribs->lastAccessTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_USERID)
         this->statData.setUserID(attribs->userID);

      if(validAttribs & SETATTR_CHANGE_GROUPID)
         this->statData.setGroupID(attribs->groupID);
   }

   bool storeRes = storeUpdatedMetaData();
   if(!storeRes)
   { // failed to update metadata on disk => restore old values
      this->statData.setSettableFileAttribs(oldAttribs);

      success = false;
   }

   safeLock.unlock();

   return success;
}


FhgfsOpsErr DirInode::setOwnerNodeID(std::string entryName, uint16_t ownerNode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
   {
      safeLock.unlock();
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   FhgfsOpsErr retVal = entries.setOwnerNodeID(entryName, ownerNode);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

