#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/MathTk.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/StorageDefinitions.h>
#include <program/Program.h>
#include "FileInode.h"

#include <attr/xattr.h>


#define FILE_STORAGE_FORMAT_VER    1

/**
 * Inode initialization.
 *
 * NOTE: Avoid to use this initilizer. It is only for not important inodes, such as fake-inodes
 *       if chunk-file unlink did not work.
 */
FileInode::FileInode(std::string entryID, int mode, unsigned userID, unsigned groupID,
   StripePattern& stripePattern, DirEntryType entryType, unsigned featureFlags,
   unsigned chunkHash) : entryID(entryID)
{
   this->globalFileStore = false;

   this->stripePattern = stripePattern.clone();
   this->parentNodeID = 0; // 0 means undefined
   this->featureFlags = 0;
   this->exclusive = false;
   this->numSessionsRead = 0;
   this->numSessionsWrite = 0;

   int64_t fileSize        = 0;
   int64_t currentTimeSecs = TimeAbs().getTimeval()->tv_sec;
   unsigned nlink          = 1; // entirely new file
   unsigned contentsVersion = 0;


   SettableFileAttribs settableAttribs = { mode, userID, groupID, currentTimeSecs,currentTimeSecs };

   this->statData.setAttribChangeTimeSecs(currentTimeSecs);
   this->statData.setModificationTimeSecs(currentTimeSecs);
   this->statData.setLastAccessTimeSecs(currentTimeSecs);

   this->statData.set(fileSize, &settableAttribs, currentTimeSecs, currentTimeSecs, nlink,
      contentsVersion);

   initFileInfoVec();

   this->dentryCompatData.entryType = entryType;
   this->dentryCompatData.featureFlags = featureFlags;

   this->chunkHash = chunkHash;
}

/**
 * Inode initialization. The preferred initializer.
 */
FileInode::FileInode(std::string entryID, StatData* statData, StripePattern& stripePattern,
   DirEntryType entryType, unsigned dentryFeatureFlags, unsigned chunkHash) : entryID(entryID)
{
   this->globalFileStore = false;

   this->stripePattern = stripePattern.clone();
   this->parentNodeID = 0; // 0 means undefined
   this->exclusive = false;
   this->numSessionsRead = 0;
   this->numSessionsWrite = 0;


   this->statData.set(statData);

   int64_t currentTimeSecs = TimeAbs().getTimeval()->tv_sec;
   this->statData.setLastAccessTimeSecs(currentTimeSecs);
   this->statData.setAttribChangeTimeSecs(currentTimeSecs);

   initFileInfoVec();

   this->dentryCompatData.entryType = entryType;
   this->dentryCompatData.featureFlags = dentryFeatureFlags;

   this->chunkHash = chunkHash;
}

/**
 * Note: This constructor does not perform the full initialization, so use it for
 * metadata loading (or similar deserialization) only.
 *
 * Note: Don't forget to call initFileInfoVec() when using this (loadFromInodeFile() includes it).
 */
FileInode::FileInode()
{
   this->stripePattern = NULL;
   this->exclusive = false;
   this->numSessionsRead = 0;
   this->numSessionsWrite = 0;
}

/**
 * Requires: init'ed stripe pattern, modification and last access time secs
 */
void FileInode::initFileInfoVec()
{
   // create a fileInfo in the vector for each stripe node

   size_t numTargets = stripePattern->getStripeTargetIDs()->size();
   unsigned chunkSize = stripePattern->getChunkSize();
   unsigned chunkSizeLog2 = MathTk::log2Int32(chunkSize);

   unsigned stripeSetSize = chunkSize * numTargets;

   int64_t lastStripeSetSize; // =fileLength%stripeSetSize (remainder after stripeSetStart)
   int64_t stripeSetStart; // =fileLength-stripeSetSize
   int64_t fullLengthPerTarget; // =stripeSetStart/numTargets (without last stripe set remainder)

   StatData* statData = &this->statData;
   int64_t fileSize = statData->getFileSize();

   /* compute stripeset start to get number of complete chunks on all nodes and stripeset remainder
      to compute each target's remainder in the last stripe set. */

   /* note: chunkSize is definitely power of two. if numTargets is also power of two, then
      stripeSetSize is also power of two */

   if(MathTk::isPowerOfTwo(numTargets) )
   { // quick path => optimized without division/modulo
      lastStripeSetSize = fileSize & (stripeSetSize-1);
      stripeSetStart = fileSize - lastStripeSetSize;
      fullLengthPerTarget = stripeSetStart >> MathTk::log2Int32(numTargets);
   }
   else
   { // slow path => requires division/modulo
      lastStripeSetSize = fileSize % stripeSetSize;
      stripeSetStart = fileSize - lastStripeSetSize;
      fullLengthPerTarget = stripeSetStart / numTargets;
   }

   // walk over all targets: compute their chunk file sizes and init timestamps

   fileInfoVec.reserve(numTargets);

   // to subtract last stripe set length of pevious targets in for-loop below
   int64_t remainingLastSetSize = lastStripeSetSize;

   for(unsigned i=0; i < numTargets; i++)
   {
      int64_t targetFileLength = fullLengthPerTarget;

      if(remainingLastSetSize > 0)
         targetFileLength += FHGFS_MIN(remainingLastSetSize, chunkSize);

      int64_t modificationTimeSecs = statData->getModificationTimeSecs();
      int64_t lastAccessTimeSecs   = statData->getLastAccessTimeSecs();

      DynamicFileAttribs dynAttribs(0, targetFileLength, modificationTimeSecs, lastAccessTimeSecs);
      StripeNodeFileInfo fileInfo(chunkSize, chunkSizeLog2, dynAttribs);

      fileInfoVec.push_back(fileInfo);

      remainingLastSetSize -= chunkSize;
   }
}

/**
 * Decrease number of sessions for read or write (=> file close) and update persistent
 * metadata.
 * Note: This currently includes persistent metadata update for efficiency reasons (because
 * we already hold the mutex lock here).
 *
 * @param accessFlags OPENFILE_ACCESS_... flags
 */
void FileInode::decNumSessionsAndStore(EntryInfo* entryInfo, unsigned accessFlags)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   if(accessFlags & OPENFILE_ACCESS_READ)
   {
      if(unlikely(!numSessionsRead) )
      {
         LogContext log("File::decNumSessionsRead");
         log.logErr(
            std::string("Warning: numSessionsRead is already zero. " +
            std::string("File: ") + entryID) );
      }
      else
         this->numSessionsRead--;
   }
   else
   { // (includes read+write)
      if(unlikely(!numSessionsWrite) )
      {
         LogContext log("File::decNumSessionsWrite");
         log.logErr(
            std::string("Warning: numSessionsWrite is already zero. " +
            std::string("File: ") + entryID) );
      }
      else
      {
         this->numSessionsWrite--;

         this->statData.setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);
      }
   }

   // dyn attribs have been updated during close, so we save them here
   storeUpdatedInode(entryInfo);

   safeLock.unlock();
}


/**
 * Note: This version is compatible with sparse files.
 */
void FileInode::updateDynamicAttribs(void)
{
   StorageTk::updateDynamicFileInodeAttribs(this->fileInfoVec, this->stripePattern, this->statData);
}

/*
 * Note: Current object state is used for the serialization
 */
unsigned FileInode::serializeMetaData(char* buf)
{
   // note: the total amount of serialized data may not be larger than META_SERBUF_SIZE

   // get latest dyn attrib values
   updateDynamicAttribs();

   DentryInodeMeta diskInodeData;
   diskInodeData.setSerializationData(this->entryID, &this->statData, this->stripePattern,
      this->chunkHash, this->featureFlags, this->dentryCompatData.featureFlags, false);

   DiskMetaData diskMetaData(this->dentryCompatData.entryType, &diskInodeData);

   unsigned retVal = diskMetaData.serializeFileInode(buf);

   return retVal;
}

/*
 * Note: Applies deserialized data directly to the current object
 */
bool FileInode::deserializeMetaData(const char* buf)
{
   this->globalFileStore = true;

   DiskMetaData diskMetaData;

   bool deserializeRes = diskMetaData.deserializeFileInode(buf);
   if (deserializeRes == false)
      return false;

   DentryInodeMeta* inodeData = diskMetaData.getInodeData();


   //  StatData
   StatData* statData = inodeData->getInodeStatData();
   statData->get(this->statData);

   // entryID
   this->entryID = inodeData->getID();

   /* Note: Will not set the parentEntryID and parentNodeID, as this inode is part of the
    * global store and usage of those values is not appropriate then (especially if the inode is
    * unlinked or a hard link with several parents... */

   // stripePattern
   this->stripePattern = inodeData->getPatternAndSetToNull();

   // featureFlags
   this->featureFlags = inodeData->getDentryFeatureFlags();


   { // dentry compat data
      // entryType
      this->dentryCompatData.entryType = diskMetaData.getEntryType();

      // (dentry) feature flags
      this->dentryCompatData.featureFlags = diskMetaData.getDentryFeatureFlags();

   }


   return true;
}



/**
 * Note: Wrapper/chooser for storeUpdatedMetaDataBufAsXAttr/Contents.
 *
 * @param buf the serialized object state that is to be stored
 * @param fd is optional
 */
bool FileInode::storeUpdatedMetaDataBuf(char* buf, unsigned bufLen, int fd)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return storeUpdatedMetaDataBufAsXAttr(buf, bufLen, fd);

   if (fd != -1)
      return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen, fd);

   return storeUpdatedMetaDataBufAsContents(buf, bufLen);
}

/**
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 * @param fd is optional
 */
bool FileInode::storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen, int fd)
{
   const char* logContext = "File (store updated xattr metadata)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), entryID);

   // write data to file
   int setRes;
   if (fd != -1)
   {
      setRes = fsetxattr(fd, META_XATTR_FILE_NAME, buf, bufLen, 0);
      close(fd);
   }
   else
      setRes = setxattr(metaFilename.c_str(), META_XATTR_FILE_NAME, buf, bufLen, 0);

   if(unlikely(setRes == -1) )
   { // error
      LogContext(logContext).logErr("Unable to write FileInode metadata update: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   LOG_DEBUG(logContext, 4, "File inode update stored: " + entryID);

   return true;
}

/**
 * Stores the update to a sparate file first and then renames it.
 *
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool FileInode::storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen)
{
   const char* logContext = "File (store updated inode)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), entryID);
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

      LogContext(logContext).logErr("Unable to create inode update file: " + metaUpdateFilename +
         ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   // metafile created => store meta data
   writeRes = write(fd, buf, bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      if( (writeRes >= 0) || (errno == ENOSPC) )
      { // no free space => try again with update in-place
         LogContext(logContext).log(Log_DEBUG, "No space left to write update inode. Trying update "
            "in-place: " + metaUpdateFilename + ". " + "SysErr: " + System::getErrString() );

         close(fd);
         unlink(metaUpdateFilename.c_str() );

         return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen);
      }

      LogContext(logContext).logErr("Unable to write inode update: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );

      goto error_closefile;
   }

   close(fd);

   renameRes = rename(metaUpdateFilename.c_str(), metaFilename.c_str() );
   if(renameRes == -1)
   {
      LogContext(logContext).logErr("Unable to replace old inode file: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );

      goto error_unlink;
   }

   LOG_DEBUG(logContext, 4, "Inode update stored: " + entryID);

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
 * @param fd is an optional existing file descriptor
 */
bool FileInode::storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen, int fd)
{
   const char* logContext = "File (store updated inode in-place)";

   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), entryID);

   int fallocRes;
   ssize_t writeRes;
   int truncRes;

   // open file (create it, but not O_EXCL because a former update could have failed)
   int openFlags = O_CREAT|O_WRONLY;

   if (fd == -1)
   {
      fd = open(metaFilename.c_str(), openFlags, 0644);
      if(fd == -1)
      { // error
         LogContext(logContext).logErr("Unable to open inode file: " + metaFilename +
            ". " + "SysErr: " + System::getErrString() );

         return false;
      }
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
            + StringTk::intToStr(bufLen)  + ") for inode update failed: " + metaFilename + ". " +
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
         + StringTk::intToStr(bufLen)  + ") for inode update failed: " +  metaFilename + ". " +
         "SysErr: " + System::getErrString(fallocRes) );
      goto error_closefile;
   }

   // metafile created => store meta data
   writeRes = write(fd, buf, bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      LogContext(logContext).logErr("Unable to write inode update: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );

      goto error_closefile;
   }

   close(fd);

   // truncate in case the update lead to a smaller file size
   truncRes = ftruncate(fd, bufLen);
   if(truncRes == -1)
   { // ignore trunc errors
      LogContext(logContext).log(Log_WARNING, "Unable to truncate inode file (strange, but "
         "proceeding anyways): " + metaFilename + ". " + "SysErr: " + System::getErrString() );
   }

   LOG_DEBUG(logContext, 4, "File inode update stored: " + entryID);

   return true;


   // error compensation
error_closefile:
   close(fd);

   return false;
}


/**
 * Update the inode on disk
 *
 * Note: We already need to have a FileInode (WRITE) rwlock here
 */
bool FileInode::storeUpdatedInode(EntryInfo* entryInfo, StripePattern* updatedStripePattern)
{
   const char* logContext = "FileInode (store updated Inode)";
   bool saveRes;

   if (this->isInlined)
   {
      FhgfsOpsErr dentrySaveRes = storeUpdatedInlinedInode(entryInfo, updatedStripePattern);
      if (dentrySaveRes == FhgfsOpsErr_SUCCESS)
         return true;

      // dentrySaveRes != FhgfsOpsErr_SUCCESS
      std::string parentID = entryInfo->getParentEntryID();
      std::string entryID  = entryInfo->getEntryID();
      std::string fileName = entryInfo->getFileName();

      if (dentrySaveRes != FhgfsOpsErr_INODENOTINLINED)
      {
         LogContext(logContext).log(Log_WARNING, std::string("Failed to write inlined inode: ") +
            "parentID: "+ parentID +  " entryID: " + entryID + " fileName: " + fileName );

         return false;
      }

      /* dentrySaveRes == FhgfsOpsErr_INODENOTINLINED. Our internal inode information says the
       * inode is inlined, but on writing it we figure out it is not. As we we are holding a
       * write lock here, that never should have happened. So probably a locking bug, but not
       * critical here and we retry using the non-inlined way.
       */

      LogContext(logContext).log(Log_WARNING, std::string("Inode unexpectedly not inlined: ") +
         "parentID: "+ parentID +  " entryID: " + entryID + " fileName: " + fileName );
      this->isInlined = false;

      // it now falls through to the not-inlined handling
   }

   // inode not inlined

   // change the stripe pattern here before serializing;

   if (unlikely(updatedStripePattern))
   {
      if (! this->stripePattern->updateStripeTargetIDs(updatedStripePattern))
         LogContext(logContext).log(Log_WARNING, "Could not set requested new stripe pattern");
   }

   char* buf = (char*)malloc(META_SERBUF_SIZE);

   unsigned bufLen = serializeMetaData(buf);

   saveRes = storeUpdatedMetaDataBuf(buf, bufLen);

   free(buf);

   return saveRes;
}

/**
 * Update an inode, which is inlined into a dentry
 */
FhgfsOpsErr FileInode::storeUpdatedInlinedInode(EntryInfo* entryInfo,
   StripePattern* updatedStripePattern)
{
   // get latest dyn attrib vals...
   updateDynamicAttribs();

   std::string parentEntryID = entryInfo->getParentEntryID();

   std::string dirEntryPath = MetaStorageTk::getMetaDirEntryPath(
         Program::getApp()->getStructurePath()->getPathAsStrConst(), parentEntryID);

   DirEntry dirEntry(entryInfo->getEntryType(), entryInfo->getFileName(),
      entryInfo->getEntryID(), entryInfo->getOwnerNodeID() );

   StatData statData;
   getStatDataUnlocked(statData);

   FhgfsOpsErr retVal = dirEntry.storeUpdatedInode(dirEntryPath, entryInfo->getEntryID(),
      &statData, updatedStripePattern);

   return retVal;
}

FhgfsOpsErr FileInode::storeInitialNonInlinedInode()
{
   const char* logContext = "Store initial inode";
   App* app = Program::getApp();
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   char* buf = (char*)malloc(META_SERBUF_SIZE);

   unsigned bufLen = serializeMetaData(buf);

   std::string idPath = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), this->updatedID);

   int openFlags = O_CREAT|O_EXCL|O_WRONLY;

   int fd = open(idPath.c_str(), openFlags, 0644);
   if(fd == -1)
   {
      if (errno != EEXIST)
         LogContext(logContext).logErr("Unable to create dentry file: " + idPath + ". " +
            "SysErr: " + System::getErrString() );

      if (errno == EMFILE)
      { /* Creating the file succeeded, but there are already too many open file descriptors to
         * open the file. We don't want to leak an entry-by-id file, so delete it.
         * We only want to delete the file for specific errors, as for example EEXIST would mean
         * we would delete an existing (probably) working entry. */
         int unlinkRes = unlink(idPath.c_str() );
         if (unlinkRes && errno != ENOENT)
            LogContext(logContext).logErr("Failed to unlink failed dentry: " + idPath + ". " +
               "SysErr: " + System::getErrString() );
      }

      if (errno == EEXIST)
         retVal = FhgfsOpsErr_EXISTS;
      else
      {
         retVal = FhgfsOpsErr_INTERNAL;
      }

      free(buf);
      return retVal;
   }


   bool saveRes = storeUpdatedMetaDataBuf(buf, bufLen, fd); // closes fd

   free(buf);

   if (saveRes == false)
      retVal = FhgfsOpsErr_INTERNAL;

   return retVal;
}


bool FileInode::removeStoredMetaData(std::string id)
{
   const char* logContext = "FileInode (remove stored metadata)";

   App* app = Program::getApp();
   std::string inodeFilename = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), id);

   // delete metadata file
   int unlinkRes = unlink(inodeFilename.c_str() );

   /* ignore errno == ENOENT as the file does not exist anymore for whatever reasons. Although
    * unlink() failed, we do not have to care, as our goal is still reached. This is also about
    * inode removal, if the dir-entry also does not exist, the application still will get
    * the right error code */
   if(unlinkRes == -1 && errno != ENOENT)
   { // error
      LogContext(logContext).logErr("Unable to delete inode file: " + inodeFilename + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   LOG_DEBUG(logContext, 4, "Inode file deleted: " + inodeFilename);

   return true;
}


/**
 * Note: Wrapper/chooser for loadFromFileXAttr/Contents.
 * Note: This also (indirectly) calls initFileInfoVec()
 */
bool FileInode::loadFromInodeFile(EntryInfo* entryInfo)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return loadFromFileXAttr(entryInfo->getEntryID() );

   return loadFromFileContents(entryInfo->getEntryID() );
}

/**
 * Note: Don't call this directly, use the wrapper loadFromInodeFile().
 * Note: This also calls initFileInfoVec()
 */
bool FileInode::loadFromFileXAttr(const std::string& id)
{
   const char* logContext = "File inode (load from xattr file)";

   App* app = Program::getApp();

   this->relInodeFilePath = StorageTkEx::getOldMetaInodePath("", id + META_SUBFILE_SUFFIX_STR_OLD);

   std::string metaFilename = app->getOldInodesPath()->getPathAsStrConst() + this->relInodeFilePath;
   this->pathToInodeFile = metaFilename;

   bool retVal = false;

   char* buf = (char*)malloc(META_SERBUF_SIZE);

   ssize_t getRes = getxattr(metaFilename.c_str(), META_XATTR_FILEINODE_NAME_OLD, buf,
      META_SERBUF_SIZE);
   if(getRes > 0)
   { // we got something => deserialize it
      bool deserRes = deserializeMetaData(buf);

      if(unlikely(!deserRes) )
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize metadata in file: " + metaFilename);
         goto error_freebuf;
      }

      // deserialization successful => init dyn attribs

#if 0 // not required for upgrade tool
      initFileInfoVec(); /* note: this can only be done after the stripePattern
         has been initialized, that's why we do it here at this "unusual" place. */
#endif

      retVal = true;
   }
   else
   { // unhandled error
      LogContext(logContext).logErr("Unable to open/read xattr inode file: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );
   }


error_freebuf:
   free(buf);

   return retVal;
}

/**
 * Note: Don't call this directly, use the wrapper loadFromInodeFile().
 * Note: This also calls initFileInfoVec()
 */
bool FileInode::loadFromFileContents(const std::string& id)
{
   const char* logContext = "File inode (load from file)";

   App* app = Program::getApp();

   this->relInodeFilePath = StorageTkEx::getOldMetaInodePath("", id + META_SUBFILE_SUFFIX_STR_OLD);

   std::string metaFilename = app->getOldInodesPath()->getPathAsStrConst() + this->relInodeFilePath;
   this->pathToInodeFile = metaFilename;

   bool retVal = false;

   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(metaFilename.c_str(), openFlags, 0);
   if(fd == -1)
   { // open failed
      LogContext(logContext).logErr("Unable to open inode file: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   char* buf = (char*)malloc(META_SERBUF_SIZE);
   int readRes = read(fd, buf, META_SERBUF_SIZE);
   if(readRes <= 0)
   { // reading failed
      LogContext(logContext).logErr("Unable to read inode file: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   {
      bool deserRes = deserializeMetaData(buf);
      if(!deserRes)
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize inode in file: " + metaFilename);
      }
      else
      { // deserialization successful => init dyn attribs
#if 0 // not required for upgrade tool
         initFileInfoVec(); // note: this can only be done after the stripePattern
            // has been initialized, that's why we do it here at this "unusual" place
#endif

         retVal = true;
      }
   }

   free(buf);
   close(fd);

   return retVal;
}

/**
 * Create an inode from an entryInfo.
 *
 * Note: The entryInfo indicates if the inode is inlined or not. However, this information
 *       might be outdated and so we need to try inlined and file-inode access, if creating
 *       the inode failed.
 *       We here rely on kernel lookup calls, to update client side entryInfo data.
 */
FileInode* FileInode::createFromEntryInfo(EntryInfo* entryInfo)
{
   FileInode* inode;

   if (entryInfo->getStatFlags() & ENTRYINFO_FLAG_INLINED)
   {
      /* entryInfo indicates the inode is inlined. So first try to get the inode by
       * dir-entry inlined inode and if that failes try again with an inode-file. */
      inode = createFromInlinedInode(entryInfo);

      if (!inode)
         inode = createFromInodeFile(entryInfo);
   }
   else
   {
      /* entryInfo indicates the inode is not inlined, but a separate inode-file. So first
       * try to get the inode by inode-file and only if that fails try again with the dir-entry,
       * maybe the inode was re-inlined.        */
      inode = createFromInodeFile(entryInfo);

      if (!inode)
         inode = createFromInlinedInode(entryInfo);
   }

   return inode;
}

/**
 * Inode from inode file.
 *
 * Note: Do not call directly, but use FileInode::createFromEntryInfo()
 */
FileInode* FileInode::createFromInodeFile(EntryInfo* entryInfo)
{
   FileInode* newInode = new FileInode();

   bool loadRes = newInode->loadFromInodeFile(entryInfo);
   if(!loadRes)
   {
      delete(newInode);

      return NULL;
   }

   if (!Upgrade::updateEntryID(newInode->entryID, newInode->updatedID) )
   {
      delete(newInode);
      return NULL;
   }

   newInode->setIsInlined(false);

   return newInode;
}


/**
 * Inode from dir-entry with inlined inode.
 *
 * Note: Do not call directly, but use FileInode::createFromEntryInfo()
 */
FileInode* FileInode::createFromInlinedInode(EntryInfo* entryInfo)
{
   std::string parentEntryID = entryInfo->getParentEntryID();

   std::string dirEntryPath = MetaStorageTk::getMetaDirEntryPath(
         Program::getApp()->getStructurePath()->getPathAsStrConst(),
         parentEntryID);

   DirEntry dirEntry(entryInfo->getEntryType(), entryInfo->getFileName(),
      entryInfo->getEntryID(), entryInfo->getOwnerNodeID() );

   FileInode* newInode = dirEntry.createInodeByID(dirEntryPath, entryInfo->getEntryID() );

   if (newInode)
      newInode->setIsInlined(true);

   return newInode;
}


/**
 * Update entry attributes like chmod() etc. do it.
 *
 * Note: modificationTimeSecs and lastAccessTimeSecs are dynamic attribs, so they require
      a special handling by the caller (but we also set the static attribs here).

 * @param validAttribs SETATTR_CHANGE_...-Flags, but maybe 0, if we only want to update
 *    AttribChangeTimeSecs.
 * @param attribs   new attributes, but may be NULL if validAttribs == 0
 */
bool FileInode::setAttrData(EntryInfo * entryInfo, int validAttribs, SettableFileAttribs* attribs)
{
   bool success = true;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   /* note: modificationTimeSecs and lastAccessTimeSecs are dynamic attribs, so they require
      a special handling by the caller (i.e. to also update chunk files) */

   // save old attribs
   SettableFileAttribs oldAttribs = *(this->statData.getSettableFileAttribs() );

   this->statData.setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);

   if(validAttribs)
   {
      // apply new attribs wrt flags...

      if(validAttribs & SETATTR_CHANGE_MODE)
         this->statData.setMode(attribs->mode);

      if(validAttribs & SETATTR_CHANGE_MODIFICATIONTIME)
      {
         /* only static value update required for storeUpdatedInode() */
         this->statData.setModificationTimeSecs(attribs->modificationTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_LASTACCESSTIME)
      {
         /* only static value update required for storeUpdatedInode() */
         this->statData.setLastAccessTimeSecs(attribs->lastAccessTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_USERID)
         this->statData.setUserID(attribs->userID);

      if(validAttribs & SETATTR_CHANGE_GROUPID)
         this->statData.setGroupID(attribs->groupID);
   }

   bool storeRes = storeUpdatedInode(entryInfo); // store on disk
   if(!storeRes)
   { // failed to update metadata on disk => restore old values
      this->statData.setSettableFileAttribs(oldAttribs);

      success = false;
      goto err_unlock;
   }

   // persistent update succeeded

   // update attribs vec (wasn't done earlier because of backup overhead for restore on error)

   if(validAttribs & SETATTR_CHANGE_MODIFICATIONTIME)
   {
      for(size_t i=0; i < fileInfoVec.size(); i++)
         fileInfoVec[i].getRawDynAttribs()->modificationTimeSecs = attribs->modificationTimeSecs;
   }

   if(validAttribs & SETATTR_CHANGE_LASTACCESSTIME)
   {
      for(size_t i=0; i < fileInfoVec.size(); i++)
         fileInfoVec[i].getRawDynAttribs()->lastAccessTimeSecs = attribs->lastAccessTimeSecs;
   }

err_unlock:
   safeLock.unlock(); // U N L O C K

   return success;
}

/**
 * General wrapper for flock lock and unlock operations.
 *
 * Note: Unlocks are always immediately granted (=> they always return "true").
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
bool FileInode::flockEntry(EntryLockDetails& lockDetails)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool retVal = flockEntryUnlocked(lockDetails);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * General wrapper for flock lock and unlock operations.
 *
 * Note: Unlocks are always immediately granted (=> they always return "true").
 * Note: Unlocked version => caller must hold write lock.
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
bool FileInode::flockEntryUnlocked(EntryLockDetails& lockDetails)
{
   bool tryNextWaiters = false;
   bool immediatelyGranted = false; // return value

   if(lockDetails.isCancel() )
   {
      // C A N C E L request

      /* note: this is typically used when a client closes a file, so we remove all granted and
         pending locks for the given handle here */

      if(flockEntryCancelByHandle(lockDetails) )
         tryNextWaiters = true;

      immediatelyGranted = true;
   }
   else
   if(lockDetails.isUnlock() )
   {
      // U N L O C K request

      tryNextWaiters = flockEntryUnlock(lockDetails);
      immediatelyGranted = true;
   }
   else
   {
      // L O C K request

      // check waiters to filter duplicate requests

      StringSetIter iterWaiters = waitersLockIDsFLock.find(lockDetails.lockAckID);
      if(iterWaiters != waitersLockIDsFLock.end() )
         return false; // re-request from waiter, but still in the queue => keep on waiting

      // not in waiters queue => is it granted already?

      bool isGrantedAlready = flockEntryIsGranted(lockDetails);
      if(isGrantedAlready)
         return true; // request was granted already

      // not waiting, not granted => we have a new request

      bool hasConflicts = flockEntryCheckConflicts(lockDetails, NULL);

      if(!hasConflicts || lockDetails.allowsWaiting() )
         tryNextWaiters = flockEntryUnlock(lockDetails); // unlock (for lock up-/downgrades)

      if(lockDetails.isShared() )
      {
         // S H A R E D lock request

         if(!hasConflicts)
         { // no confictors for this lock => can be immediately granted
            flockEntryShared(lockDetails);
            immediatelyGranted = true;
         }
         else
         if(lockDetails.allowsWaiting() )
         { // we have conflictors and locker wants to wait
            waitersSharedFLock.push_back(lockDetails);
            waitersLockIDsFLock.insert(lockDetails.lockAckID);
         }
      }
      else
      {
         // E X C L U S I V E lock request

         if(!hasConflicts)
         { // no confictors for this lock => can be immediately granted
            flockEntryExclusive(lockDetails);
            immediatelyGranted = true;
         }
         else
         if(lockDetails.allowsWaiting() )
         { // we have conflictors and locker wants to wait
            waitersExclFLock.push_back(lockDetails);
            waitersLockIDsFLock.insert(lockDetails.lockAckID);
         }
      }
   }

   if(tryNextWaiters)
      flockEntryTryNextWaiters();

   return immediatelyGranted;
}

/**
 * Remove all waiters from the queues.
 */
void FileInode::flockEntryCancelAllWaiters()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   waitersLockIDsFLock.clear();
   waitersExclFLock.clear();
   waitersSharedFLock.clear();

   safeLock.unlock(); // U N L O C K
}


/**
 * Unlock all locks and wait entries of the given clientID.
 */
void FileInode::flockEntryCancelByClientID(std::string clientID)
{
   /* note: this code is in many aspects similar to flockEntryCancelByHandle(), so if you change
    * something here, you probably want to change it there, too. */


   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool tryNextWaiters = false;

   // exclusive lock

   if(exclFLock.isSet() && (exclFLock.clientID == clientID) )
   {
      exclFLock.unset();
      tryNextWaiters = true;
   }

   // shared locks

   for(EntryLockDetailsSetIter iter = sharedFLocks.begin();
       iter != sharedFLocks.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientID == clientID)
      {
         EntryLockDetailsSetIter iterNext = iter;
         iterNext++;

         sharedFLocks.erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters exlusive

   for(EntryLockDetailsListIter iter = waitersExclFLock.begin();
       iter != waitersExclFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientID == clientID)
      {
         waitersLockIDsFLock.erase(iter->lockAckID);
         iter = waitersExclFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters shared

   for(EntryLockDetailsListIter iter = waitersSharedFLock.begin();
       iter != waitersSharedFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientID == clientID)
      {
         waitersLockIDsFLock.erase(iter->lockAckID);
         iter = waitersSharedFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }


   if(tryNextWaiters)
      flockEntryTryNextWaiters();

   safeLock.unlock(); // U N L O C K
}

/**
 * Remove all granted and pending locks that match the given handle.
 * (This is typically called by clients during file close.)
 *
 * Note: unlocked, so hold the mutex when calling this.
 *
 * @return true if locks were removed and next waiters should be tried.
 */
bool FileInode::flockEntryCancelByHandle(EntryLockDetails& lockDetails)
{
   /* note: this code is in many aspects similar to flockEntryCancelByClientID(), so if you change
    * something here, you probably want to change it there, too. */


   bool tryNextWaiters = false;

   // exclusive lock

   if(exclFLock.isSet() && lockDetails.equalsHandle(exclFLock) )
   {
      exclFLock.unset();
      tryNextWaiters = true;
   }

   // shared locks

   for(EntryLockDetailsSetIter iter = sharedFLocks.begin();
       iter != sharedFLocks.end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         EntryLockDetailsSetIter iterNext = iter;
         iterNext++;

         sharedFLocks.erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters exlusive

   for(EntryLockDetailsListIter iter = waitersExclFLock.begin();
       iter != waitersExclFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         waitersLockIDsFLock.erase(iter->lockAckID);
         iter = waitersExclFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters shared

   for(EntryLockDetailsListIter iter = waitersSharedFLock.begin();
       iter != waitersSharedFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         waitersLockIDsFLock.erase(iter->lockAckID);
         iter = waitersSharedFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   return tryNextWaiters;
}

/**
 * Note: Automatically ignores self-conflicts (locks that could be up- or downgraded).
 * Note: Make sure to remove lock duplicates before calling this.
 * Note: unlocked, so hold the mutex when calling this.
 *
 * @param outConflictor first identified conflicting lock (only set if true is returned; can be
 * NULL if caller is not interested)
 * @return true if there is a conflict with a lock that is not owned by the current lock requestor,
 * false if the request can defintely be granted immediately without waiting
 */
bool FileInode::flockEntryCheckConflicts(EntryLockDetails& lockDetails, EntryLockDetails* outConflictor)
{
   // note: we also check waiting writers here, because we have writer preference and so we don't
      // want to grant access for a new reader if we have a waiting writer


   // check conflicting exclusive lock (for shared & eclusive requests)

   if(exclFLock.isSet() && !exclFLock.equalsHandle(lockDetails) )
   {
      SAFE_ASSIGN(outConflictor, exclFLock);
      return true;
   }

   // no exclusive lock exists

   if(lockDetails.isExclusive() )
   { // exclusive lock request: check conflicting shared lock

      for(EntryLockDetailsSetCIter iterShared = sharedFLocks.begin();
          iterShared != sharedFLocks.end();
          iterShared++)
      {
         if(!iterShared->equalsHandle(lockDetails) )
         { // found a conflicting lock
            SAFE_ASSIGN(outConflictor, *iterShared);
            return true;
         }
      }
   }
   else
   { // non-exclusive lock: check for waiting writers to enforce writer preference

      if(!waitersExclFLock.empty() )
      {
         SAFE_ASSIGN(outConflictor, *waitersExclFLock.begin() );
         return true;
      }
   }

   return false;
}

/**
 * Find out whether a given lock is currently being held by the given owner.
 *
 * Note: unlocked, hold the read lock when calling this.
 *
 * @return true if the given lock is being held by the given owner.
 */
bool FileInode::flockEntryIsGranted(EntryLockDetails& lockDetails)
{
   if(lockDetails.isExclusive() )
   {
      if(exclFLock.equalsHandle(lockDetails) )
      { // was an exclusive lock
         return true;
      }
   }
   else
   if(lockDetails.isShared() )
   {
      EntryLockDetailsSetIter iterShared = sharedFLocks.find(lockDetails);
      if(iterShared != sharedFLocks.end() )
      { // was a shared lock
         return true;
      }
   }

   return false;
}


/**
 * Note: unlocked, so hold the write lock when calling this.
 *
 * @return true if an existing lock was released
 */
bool FileInode::flockEntryUnlock(EntryLockDetails& lockDetails)
{
   if(exclFLock.equalsHandle(lockDetails) )
   { // was an exclusive lock
      exclFLock.unset();

      return true;
   }

   EntryLockDetailsSetIter iterShared = sharedFLocks.find(lockDetails);
   if(iterShared != sharedFLocks.end() )
   { // was a shared lock
      sharedFLocks.erase(iterShared);

      return true;
   }

   return false;
}


/**
 * Note: We assume that unlock() has been called before, so we don't check for up-/downgrades or
 * duplicates.
 * Note: unlocked, so hold the mutex when calling this
 */
void FileInode::flockEntryShared(EntryLockDetails& lockDetails)
{
   sharedFLocks.insert(lockDetails);
}

/**
 * Note: We assume that unlock() has been called before, so we don't check for up-/downgrades or
 * duplicates.
 * Note: unlocked, so hold the mutex when calling this
 */
void FileInode::flockEntryExclusive(EntryLockDetails& lockDetails)
{
   exclFLock = lockDetails;
}


/**
 * Remove next requests from waiters queue and try to grant it - until we reach an entry that
 * cannot be granted immediately.
 *
 * Note: We assume that duplicate waiters and duplicate granted locks (up-/downgrades) have been
 * removed before a lock request is enqueued, so we don't check for that.
 * Note: unlocked, so hold the mutex when calling this.
 */
void FileInode::flockEntryTryNextWaiters()
{
   /* note: we have writer preference, so we don't grant any new readers while we have waiting
      writers */

   if(exclFLock.isSet() )
      return; // eclusive lock => there's nothing we can do right now

   // no exclusive lock set

   if(!waitersSharedFLock.empty() && waitersExclFLock.empty() )
   { // shared locks waiting and no exclusive locks waiting => grant all

      LockEntryNotifyList* notifyList = new LockEntryNotifyList();

      while(!waitersSharedFLock.empty() )
      {
         flockEntryShared(*waitersSharedFLock.begin() );

         notifyList->push_back(*waitersSharedFLock.begin() );

         waitersLockIDsFLock.erase(waitersSharedFLock.begin()->lockAckID);
         waitersSharedFLock.pop_front();
      }

      LockingNotifier::notifyWaiters(this->parentEntryID, this->entryID, notifyList);

      return;
   }

   // no exclusive and no shared locks set => we can grant an exclusive lock

   if(!waitersExclFLock.empty() )
   { // exclusive locks waiting => grant first one of them
      flockEntryExclusive(*waitersExclFLock.begin() );

      LockEntryNotifyList* notifyList = new LockEntryNotifyList();
      notifyList->push_back(*waitersExclFLock.begin() );
      LockingNotifier::notifyWaiters(this->parentEntryID, this->entryID, notifyList);

      waitersLockIDsFLock.erase(waitersExclFLock.begin()->lockAckID);
      waitersExclFLock.pop_front();
   }
}

/**
 * Generate a complete locking status overview (all granted and waiters) as human-readable string.
 */
std::string FileInode::flockEntryGetAllAsStr()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   std::ostringstream outStream;

   outStream << "Exclusive" << std::endl;
   outStream << "=========" << std::endl;
   if(exclFLock.isSet() )
      outStream << exclFLock.toString() << std::endl;

   outStream << std::endl;

   outStream << "Shared" << std::endl;
   outStream << "=========" << std::endl;
   for(EntryLockDetailsSetCIter iter = sharedFLocks.begin();
       iter != sharedFLocks.end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Exclusive Waiters" << std::endl;
   outStream << "=========" << std::endl;
   for(EntryLockDetailsListCIter iter = waitersExclFLock.begin();
       iter != waitersExclFLock.end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Shared Waiters" << std::endl;
   outStream << "=========" << std::endl;
   for(EntryLockDetailsListCIter iter = waitersSharedFLock.begin();
       iter != waitersSharedFLock.end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Waiters lockIDs" << std::endl;
   outStream << "=========" << std::endl;
   for(StringSetCIter iter = waitersLockIDsFLock.begin();
       iter != waitersLockIDsFLock.end();
       iter++)
   {
      outStream << *iter << std::endl;
   }

   outStream << std::endl;

   safeLock.unlock(); // U N L O C K

   return outStream.str();
}

/**
 * General wrapper for flock lock and unlock operations.
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
bool FileInode::flockRange(RangeLockDetails& lockDetails)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool retVal = flockRangeUnlocked(lockDetails);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * General wrapper for flock lock and unlock operations.
 *
 * Note: Unlocked, so caller must hold the write lock.
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
bool FileInode::flockRangeUnlocked(RangeLockDetails& lockDetails)
{
   bool tryNextWaiters = false;
   bool immediatelyGranted = false; // return value

   if(lockDetails.isCancel() )
   {
      // C A N C E L request

      /* note: this is typically used when a client closes a file, so we remove all granted and
         pending locks for the given handle here */

      if(flockRangeCancelByHandle(lockDetails) )
         tryNextWaiters = true;

      immediatelyGranted = true;
   }
   else
   if(lockDetails.isUnlock() )
   {
      // U N L O C K request

      tryNextWaiters = flockRangeUnlock(lockDetails);
      immediatelyGranted = true;
   }
   else
   {
      // L O C K request

      // check waiters to filter duplicate requests

      StringSetIter iterWaiters = waitersLockIDsRangeFLock.find(lockDetails.lockAckID);
      if(iterWaiters != waitersLockIDsRangeFLock.end() )
         return false; // re-request from waiter, but still in the queue => keep on waiting

      // not in waiters queue => is it granted already?

      bool isGrantedAlready = flockRangeIsGranted(lockDetails);
      if(isGrantedAlready)
         return true; // request was granted already

      // not waiting, not granted => we have a new request

      bool hasConflicts = flockRangeCheckConflicts(lockDetails, NULL);

      if(!hasConflicts || lockDetails.allowsWaiting() )
         tryNextWaiters = flockRangeUnlock(lockDetails); // unlock range (for lock up-/downgrades)

      if(lockDetails.isShared() )
      {
         // S H A R E D lock request

         if(!hasConflicts)
         { // no confictors for this lock => can be immediately granted
            flockRangeShared(lockDetails);
            immediatelyGranted = true;
         }
         else
         if(lockDetails.allowsWaiting() )
         { // we have conflictors and locker wants to wait
            waitersSharedRangeFLock.push_back(lockDetails);
            waitersLockIDsRangeFLock.insert(lockDetails.lockAckID);
         }
      }
      else
      {
         // E X C L U S I V E lock request

         if(!hasConflicts)
         { // no confictors for this lock => can be immediately granted
            flockRangeExclusive(lockDetails);
            immediatelyGranted = true;
         }
         else
         if(lockDetails.allowsWaiting() )
         { // we have conflictors and locker wants to wait
            waitersExclRangeFLock.push_back(lockDetails);
            waitersLockIDsRangeFLock.insert(lockDetails.lockAckID);
         }
      }
   }

   if(tryNextWaiters)
      flockRangeTryNextWaiters();


   return immediatelyGranted;
}

/**
 * Remove all waiters from the queues.
 */
void FileInode::flockRangeCancelAllWaiters()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   waitersLockIDsRangeFLock.clear();
   waitersExclRangeFLock.clear();
   waitersSharedRangeFLock.clear();

   safeLock.unlock(); // U N L O C K
}

/**
 * Unlock all locks and wait entries of the given clientID.
 */
void FileInode::flockRangeCancelByClientID(std::string clientID)
{
   /* note: this code is in many aspects similar to flockRangeCancelByHandle(), so if you change
    * something here, you probably want to change it there, too. */


   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool tryNextWaiters = false;

   // exclusive locks

   for(RangeLockExclSetIter iter = exclRangeFLocks.begin();
       iter != exclRangeFLocks.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientID == clientID)
      {
         RangeLockExclSetIter iterNext = iter;
         iterNext++;

         exclRangeFLocks.erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // shared locks

   for(RangeLockSharedSetIter iter = sharedRangeFLocks.begin();
       iter != sharedRangeFLocks.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientID == clientID)
      {
         RangeLockSharedSetIter iterNext = iter;
         iterNext++;

         sharedRangeFLocks.erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters exlusive

   for(RangeLockDetailsListIter iter = waitersExclRangeFLock.begin();
       iter != waitersExclRangeFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientID == clientID)
      {
         waitersLockIDsRangeFLock.erase(iter->lockAckID);
         iter = waitersExclRangeFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters shared

   for(RangeLockDetailsListIter iter = waitersSharedRangeFLock.begin();
       iter != waitersSharedRangeFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientID == clientID)
      {
         waitersLockIDsRangeFLock.erase(iter->lockAckID);
         iter = waitersSharedRangeFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }


   if(tryNextWaiters)
      flockRangeTryNextWaiters();

   safeLock.unlock(); // U N L O C K
}

/**
 * Remove all granted and pending locks that match the given handle.
 * (This is typically called by clients during file close.)
 *
 * Note: unlocked, so hold the mutex when calling this.
 *
 * @return true if locks were removed and next waiters should be tried.
 */
bool FileInode::flockRangeCancelByHandle(RangeLockDetails& lockDetails)
{
   /* note: this code is in many aspects similar to flockRangeCancelByClientID(), so if you change
    * something here, you probably want to change it there, too. */


   bool tryNextWaiters = false;

   // exclusive locks

   for(RangeLockExclSetIter iter = exclRangeFLocks.begin();
       iter != exclRangeFLocks.end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         RangeLockExclSetIter iterNext = iter;
         iterNext++;

         exclRangeFLocks.erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // shared locks

   for(RangeLockSharedSetIter iter = sharedRangeFLocks.begin();
       iter != sharedRangeFLocks.end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         RangeLockSharedSetIter iterNext = iter;
         iterNext++;

         sharedRangeFLocks.erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters exlusive

   for(RangeLockDetailsListIter iter = waitersExclRangeFLock.begin();
       iter != waitersExclRangeFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         waitersLockIDsRangeFLock.erase(iter->lockAckID);
         iter = waitersExclRangeFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters shared

   for(RangeLockDetailsListIter iter = waitersSharedRangeFLock.begin();
       iter != waitersSharedRangeFLock.end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         waitersLockIDsRangeFLock.erase(iter->lockAckID);
         iter = waitersSharedRangeFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }


   return tryNextWaiters;
}


/**
 * Checks if there is a conflict for the given lock (but does not actually place lock).
 *
 * @param outConflictor the conflicting lock (or of of them) in case we return true.
 * @return true if there is a conflict for the given lock request.
 */
bool FileInode::flockRangeGetConflictor(RangeLockDetails& lockDetails, RangeLockDetails* outConflictor)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   bool hasConflicts = flockRangeCheckConflicts(lockDetails, outConflictor);

   safeLock.unlock(); // U N L O C K

   return hasConflicts;
}

/**
 * Note: see flockRangeCheckConflictsEx() for comments (this is just the simple version which
 * checks the whole excl waiters queue and hence is inappropriate for tryNextWaiters() ).
 */
bool FileInode::flockRangeCheckConflicts(RangeLockDetails& lockDetails, RangeLockDetails* outConflictor)
{
   return flockRangeCheckConflictsEx(lockDetails, -1, outConflictor);
}


/**
 * Note: Automatically ignores self-conflicts (locks that could be up- or downgraded)
 * Note: unlocked, so hold the mutex when calling this
 *
 * @param outConflictor first identified conflicting lock (only set if true is returned; can be
 * NULL if caller is not interested)
 * @param maxExclWaitersCheckNum only required by tryNextWaiters to find out how many pending excls
 * in the queue before the checked element should be tested for conflicts (ie for the 5th queue
 * element you will pass 4 here); -1 will check the whole queue, which is what all other callers
 * probably want to do.
 * @return true if there is a conflict with a lock that is not owned by the current lock requestor
 */
bool FileInode::flockRangeCheckConflictsEx(RangeLockDetails& lockDetails, int maxExclWaitersCheckNum,
   RangeLockDetails* outConflictor)
{
   // note: we also check waiting writers here, because we have writer preference and so we don't
      // want to grant access for a new reader if we have a waiting writer
      // ...and we also don't want to starve writers by other writers, so we also check for
      // overlapping waiting writer requests before granting a write lock


   // check conflicting exclusive locks (for shared & exclusive requests)

   for(RangeLockExclSetCIter iterExcl = exclRangeFLocks.begin();
       (iterExcl != exclRangeFLocks.end() ) && (iterExcl->start <= lockDetails.end);
       iterExcl++)
   {
      if(lockDetails.overlaps(*iterExcl) &&
         !lockDetails.equalsHandle(*iterExcl) )
      {
         SAFE_ASSIGN(outConflictor, *iterExcl);
         return true;
      }
   }

   // no conflicting exclusive lock exists

   if(lockDetails.isExclusive() )
   { // exclusive lock request: check conflicting shared locks

      // check granted shared locks

      for(RangeLockSharedSetCIter iterShared = sharedRangeFLocks.begin();
          iterShared != sharedRangeFLocks.end();
          iterShared++)
      {
         if(lockDetails.overlaps(*iterShared) &&
            !lockDetails.equalsHandle(*iterShared) )
         {
            SAFE_ASSIGN(outConflictor, *iterShared);
            return true;
         }
      }
   }

   // no conflicting shared lock exists

   // check waiting writers (for shared reqs to prefer writers and for excl reqs to avoid
      // writer starvation of partially overlapping waiting writers)

   // (note: keep in mind that maxExclWaitersCheckNum can also be -1 for infinite checks)

   for(RangeLockDetailsListCIter iter = waitersExclRangeFLock.begin();
       (iter != waitersExclRangeFLock.end() ) && (maxExclWaitersCheckNum != 0);
       iter++, maxExclWaitersCheckNum--)
   {
      if(lockDetails.overlaps(*iter) &&
         !lockDetails.equalsHandle(*iter) )
      {
         SAFE_ASSIGN(outConflictor, *iter);
         return true;
      }
   }



   return false;
}


/**
 * Note: We assume that unlock() has been called before, so we don't check for up-/downgrades or
 * duplicates.
 * Note: unlocked, so hold the mutex when calling this
 */
void FileInode::flockRangeShared(RangeLockDetails& lockDetails)
{
   // insert shared lock request...
   // (avoid duplicates and side-by-side locks for same file handles by merging)

   for(RangeLockSharedSetIter iterShared = sharedRangeFLocks.begin();
       iterShared != sharedRangeFLocks.end();
       /* conditional iter increment inside loop */ )
   {
      bool incIterAtEnd = true;

      if(lockDetails.equalsHandle(*iterShared) && lockDetails.isMergeable(*iterShared) )
      { // same handle => merge with existing lock

         // note: all overlaps will be merged into lockDetails, so every other overlapping entry
            // can be removed here

         lockDetails.merge(*iterShared);

         RangeLockExclSetIter iterSharedNext(iterShared);
         iterSharedNext++;

         sharedRangeFLocks.erase(iterShared);

         iterShared = iterSharedNext;
         incIterAtEnd = false;
      }

      if(incIterAtEnd)
         iterShared++;
   }

   // actually insert the new lock
   sharedRangeFLocks.insert(lockDetails);
}

/**
 * Note: We assume that unlock() has been called before, so we don't check for up-/downgrades or
 * duplicates.
 * Note: unlocked, so hold the mutex when calling this
 */
void FileInode::flockRangeExclusive(RangeLockDetails& lockDetails)
{
   // insert excl lock request...
   // (avoid duplicates and side-by-side locks for same file handles by merging)

   // (note: lockDetails.end+1: because we're also looking for extensions, not only overlaps)
   for(RangeLockExclSetIter iterExcl = exclRangeFLocks.begin();
       (iterExcl != exclRangeFLocks.end() ) && (iterExcl->start <= (lockDetails.end+1) );
       /* conditional iter increment inside loop */ )
   {
      bool incIterAtEnd = true;

      if(lockDetails.equalsHandle(*iterExcl) && lockDetails.isMergeable(*iterExcl) )
      { // same handle => merge with existing lock

         // note: all overlaps will be merged into lockDetails, so every other overlapping entry
            // can be removed here

         lockDetails.merge(*iterExcl);

         RangeLockExclSetIter iterExclNext(iterExcl);
         iterExclNext++;

         exclRangeFLocks.erase(iterExcl);

         iterExcl = iterExclNext;
         incIterAtEnd = false;
      }

      if(incIterAtEnd)
         iterExcl++;
   }

   // actually insert the new lock
   exclRangeFLocks.insert(lockDetails);
}

/**
 * Find out whether a given range lock is currently being held by the given owner.
 *
 * Note: unlocked, hold the read lock when calling this.
 *
 * @return true if the range is locked by the given owner
 */
bool FileInode::flockRangeIsGranted(RangeLockDetails& lockDetails)
{
   if(lockDetails.isExclusive() )
   {
      for(RangeLockExclSetIter iterExcl = exclRangeFLocks.begin();
          (iterExcl != exclRangeFLocks.end() ) && (iterExcl->start <= lockDetails.end);
          /* conditional iter increment at end of loop */ )
      {
         if(!lockDetails.equalsHandle(*iterExcl) )
         { // lock owned by another client/process
            iterExcl++;
            continue;
         }

         // found a lock that is owned by the same client/process => check overlap with given lock

         bool incIterAtEnd = true;

         RangeOverlapType overlap = lockDetails.overlapsEx(*iterExcl);

         switch(overlap)
         {
            case RangeOverlapType_EQUALS:
            { // found an exact match => don't need to look any further
               return true;
            } break;

            case RangeOverlapType_ISCONTAINED:
            { /* given range is fully contained in a greater locked area => don't need to look any
                 further */
               return true;
            } break;

            case RangeOverlapType_CONTAINS:
            { /* found a range which is part of the given lock => given owner cannot currently hold
                 the lock for the whole given range, otherwise we wouldn't find a partial match
                 because of our merging => don't need to look any further */

               return false;
            } break;

            case RangeOverlapType_STARTOVERLAP:
            case RangeOverlapType_ENDOVERLAP:
            { /* found a range which is part of the given lock => given owner cannot currently hold
                 the lock for the whole given range, otherwise we wouldn't find a partial match
                 because of our merging => don't need to look any further */

               return false;
            } break;

            default: break; // no overlap

         } // end of switch(overlap)

         if(incIterAtEnd)
            iterExcl++;
      }
   } // end of exclusive locks check
   else
   if(lockDetails.isShared() )
   {
      for(RangeLockSharedSetIter iterShared = sharedRangeFLocks.begin();
          iterShared != sharedRangeFLocks.end();
          /* conditional iter increment at end of loop */ )
      {
         if(!lockDetails.equalsHandle(*iterShared) )
         { // lock owned by another client/process
            iterShared++;
            continue;
         }

         // found a lock that is owned by the same client/process => check overlap with given lock

         bool incIterAtEnd = true;

         RangeOverlapType overlap = lockDetails.overlapsEx(*iterShared);

         switch(overlap)
         {
            case RangeOverlapType_EQUALS:
            { // found an exact match => don't need to look any further

               return true;
            } break;

            case RangeOverlapType_ISCONTAINED:
            { /* given lock is fully contained in a greater locked area => don't need to look any
                 further */

               return true;
            } break;

            case RangeOverlapType_CONTAINS:
            { /* found a range which is part of the given lock => given owner cannot currently hold
                 the lock for the whole given range, otherwise we wouldn't find a partial match
                 because of our merging => don't need to look any further */

               return false;
            } break;

            case RangeOverlapType_STARTOVERLAP:
            case RangeOverlapType_ENDOVERLAP:
            {  /* found a range which is part of the given lock => given owner cannot currently hold
                 the lock for the whole given range, otherwise we wouldn't find a partial match
                 because of our merging => don't need to look any further */

               return false;
            } break;

            default: break; // no overlap

         } // end of switch(overlap)

         if(incIterAtEnd)
            iterShared++;
      }
   } // end of shared locks check


   return false;
}


/**
 * Note: unlocked, so hold the mutex when calling this.
 *
 * @return true if an existing lock has been removed
 */
bool FileInode::flockRangeUnlock(RangeLockDetails& lockDetails)
{
   bool lockRemoved = false; // return value

   // check exclusive locks...
   // (quick path: if the whole unlock is entirely covered by an exclusive range, then we don't need
   // to look any further)

   for(RangeLockExclSetIter iterExcl = exclRangeFLocks.begin();
       (iterExcl != exclRangeFLocks.end() ) && (iterExcl->start <= lockDetails.end);
       /* conditional iter increment at end of loop */ )
   {
      if(!lockDetails.equalsHandle(*iterExcl) )
      { // lock owned by another client/process
         iterExcl++;
         continue;
      }

      // found a lock that is owned by the same client/process => check overlap with unlock request

      bool incIterAtEnd = true;

      RangeOverlapType overlap = lockDetails.overlapsEx(*iterExcl);

      switch(overlap)
      {
         case RangeOverlapType_EQUALS:
         { // found an exact match => don't need to look any further
            exclRangeFLocks.erase(iterExcl);

            return true;
         } break;

         case RangeOverlapType_ISCONTAINED:
         { // unlock is fully contained in a greater locked area => don't need to look any further

            // check if 1 or 2 locked areas remain (=> shrink or split)

            if( (lockDetails.start == iterExcl->start) ||
                (lockDetails.end == iterExcl->end) )
            { // only one locked area remains
               RangeLockDetails oldExcl(*iterExcl);
               oldExcl.trim(lockDetails);

               exclRangeFLocks.erase(iterExcl);
               exclRangeFLocks.insert(oldExcl);
            }
            else
            { // two locked areas remain
               RangeLockDetails oldExcl(*iterExcl);
               RangeLockDetails newExcl;

               oldExcl.split(lockDetails, newExcl);

               exclRangeFLocks.erase(iterExcl);
               exclRangeFLocks.insert(oldExcl);
               exclRangeFLocks.insert(newExcl);
            }

            return true;
         } break;

         case RangeOverlapType_CONTAINS:
         { // full removal of this lock, but there may still be some others that need to be removed
            RangeLockExclSetIter iterExclNext(iterExcl);
            iterExclNext++;

            exclRangeFLocks.erase(iterExcl);

            lockRemoved = true;

            iterExcl = iterExclNext;
            incIterAtEnd = false;
         } break;

         case RangeOverlapType_STARTOVERLAP:
         case RangeOverlapType_ENDOVERLAP:
         { // partial removal of this lock and there may still be others that need to be removed
            // note: might change start and consequently map position => re-insert excl lock
            RangeLockExclSetIter iterExclNext(iterExcl);
            iterExclNext++;

            RangeLockDetails oldExcl(*iterExcl);
            oldExcl.trim(lockDetails);

            exclRangeFLocks.erase(iterExcl);
            exclRangeFLocks.insert(oldExcl);

            lockRemoved = true;

            iterExcl = iterExclNext;
            incIterAtEnd = false;
         } break;

         default: break; // no overlap

      } // end of switch(overlap)

      if(incIterAtEnd)
         iterExcl++;
   }

   // check shared locks...
   // (similar to exclusive locks, we can stop here if unlock is entirely covered by one of our
   // owned shared ranges, because there cannot be another overlapping range which we also own)

   for(RangeLockSharedSetIter iterShared = sharedRangeFLocks.begin();
       iterShared != sharedRangeFLocks.end();
       /* conditional iter increment at end of loop */ )
   {
      if(!lockDetails.equalsHandle(*iterShared) )
      { // lock owned by another client/process
         iterShared++;
         continue;
      }

      // found a lock that is owned by the same client/process => check overlap with unlock request

      bool incIterAtEnd = true;

      RangeOverlapType overlap = lockDetails.overlapsEx(*iterShared);

      switch(overlap)
      {
         case RangeOverlapType_EQUALS:
         { // found an exact match => don't need to look any further
            sharedRangeFLocks.erase(iterShared);

            return true;
         } break;

         case RangeOverlapType_ISCONTAINED:
         { // unlock is fully contained in a greater locked area => don't need to look any further

            // check if 1 or 2 locked areas remain...

            if( (lockDetails.start == iterShared->start) ||
                (lockDetails.end == iterShared->end) )
            { // only one locked area remains
               RangeLockDetails oldShared(*iterShared);
               oldShared.trim(lockDetails);

               sharedRangeFLocks.erase(iterShared);
               sharedRangeFLocks.insert(oldShared);
            }
            else
            { // two locked areas remain
               RangeLockDetails oldShared(*iterShared);
               RangeLockDetails newShared;

               oldShared.split(lockDetails, newShared);

               sharedRangeFLocks.erase(iterShared);
               sharedRangeFLocks.insert(oldShared);
               sharedRangeFLocks.insert(newShared);
            }

            return true;
         } break;

         case RangeOverlapType_CONTAINS:
         { // full removal of this lock, but there may still be some others that need to be removed
            RangeLockExclSetIter iterExclNext(iterShared);
            iterExclNext++;

            sharedRangeFLocks.erase(iterShared);

            lockRemoved = true;

            iterShared = iterExclNext;
            incIterAtEnd = false;
         } break;

         case RangeOverlapType_STARTOVERLAP:
         case RangeOverlapType_ENDOVERLAP:
         { // partial removal of this lock and there may still be others that need to be removed
            // note: might change start and consequently map position => re-insert excl lock
            RangeLockExclSetIter iterSharedNext(iterShared);
            iterSharedNext++;

            RangeLockDetails oldShared(*iterShared);
            oldShared.trim(lockDetails);

            sharedRangeFLocks.erase(iterShared);
            sharedRangeFLocks.insert(oldShared);

            lockRemoved = true;

            iterShared = iterSharedNext;
            incIterAtEnd = false;
         } break;

         default: break; // no overlap

      } // end of switch(overlap)

      if(incIterAtEnd)
         iterShared++;
   }


   return lockRemoved;
}

/**
 * Remove next requests from waiters queue and try to grant it - until we reach an entry that
 * cannot be granted immediately.
 *
 * Note: unlocked, so hold the mutex when calling this.
 */
void FileInode::flockRangeTryNextWaiters()
{
   int numWaitersBefore = 0; // number of waiters in the queue before the current checked element

   LockRangeNotifyList notifyList; // quick stack version to speed up the no waiter granted path


   for(RangeLockDetailsListIter iter = waitersExclRangeFLock.begin();
       iter != waitersExclRangeFLock.end();
       /* conditional iter inc inside loop */)
   {
      bool hasConflict = flockRangeCheckConflictsEx(*iter, numWaitersBefore, NULL);
      if(hasConflict)
      {
         iter++;
         numWaitersBefore++;
         continue;
      }

      // no conflict => grant lock

      flockRangeExclusive(*iter);

      notifyList.push_back(*iter);

      waitersLockIDsRangeFLock.erase(iter->lockAckID);
      iter = waitersExclRangeFLock.erase(iter);
   }

   for(RangeLockDetailsListIter iter = waitersSharedRangeFLock.begin();
       iter != waitersSharedRangeFLock.end();
       /* conditional iter inc inside loop */)
   {
      bool hasConflict = flockRangeCheckConflicts(*iter, NULL);
      if(hasConflict)
      {
         iter++;
         continue;
      }

      // no conflict => grant lock

      flockRangeShared(*iter);

      notifyList.push_back(*iter);

      waitersLockIDsRangeFLock.erase(iter->lockAckID);
      iter = waitersSharedRangeFLock.erase(iter);
   }

   // notify waiters

   if(!notifyList.empty() )
   { // granted new locks => create heap notifyList
      LockRangeNotifyList* allocedNotifyList = new LockRangeNotifyList();

      allocedNotifyList->swap(notifyList);

      LockingNotifier::notifyWaiters(this->parentEntryID, this->entryID, allocedNotifyList);
   }
}


/**
 * Generate a complete locking status overview (all granted and waiters) as human-readable string.
 */
std::string FileInode::flockRangeGetAllAsStr()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   std::ostringstream outStream;

   outStream << "Exclusive" << std::endl;
   outStream << "=========" << std::endl;
   for(RangeLockExclSetCIter iter = exclRangeFLocks.begin();
       iter != exclRangeFLocks.end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Shared" << std::endl;
   outStream << "=========" << std::endl;
   for(RangeLockSharedSetCIter iter = sharedRangeFLocks.begin();
       iter != sharedRangeFLocks.end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Exclusive Waiters" << std::endl;
   outStream << "=========" << std::endl;
   for(RangeLockDetailsListCIter iter = waitersExclRangeFLock.begin();
       iter != waitersExclRangeFLock.end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Shared Waiters" << std::endl;
   outStream << "=========" << std::endl;
   for(RangeLockDetailsListCIter iter = waitersSharedRangeFLock.begin();
       iter != waitersSharedRangeFLock.end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Waiters lockIDs" << std::endl;
   outStream << "=========" << std::endl;
   for(StringSetCIter iter = waitersLockIDsRangeFLock.begin();
       iter != waitersLockIDsRangeFLock.end();
       iter++)
   {
      outStream << *iter << std::endl;
   }

   outStream << std::endl;

   safeLock.unlock(); // U N L O C K

   return outStream.str();
}
