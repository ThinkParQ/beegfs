#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/MathTk.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/StorageDefinitions.h>
#include <toolkit/XAttrTk.h>
#include <program/Program.h>
#include "FileInode.h"
#include "Locking.h"

#include <sys/xattr.h>

#include <boost/lexical_cast.hpp>


// shorthand for the long init line of AppendLockQueuesContainer to create on stack
#define FILEINODE_APPEND_LOCK_QUEUES_CONTAINER(varName) \
   AppendLockQueuesContainer varName( \
      &exclAppendLock, &waitersExclAppendLock, &waitersLockIDsAppendLock)

// shorthand for the long init line of EntryLockQueuesContainer to create on stack
#define FILEINODE_ENTRY_LOCK_QUEUES_CONTAINER(varName) \
   EntryLockQueuesContainer varName( \
      &exclFLock, &sharedFLocks, &waitersExclFLock, &waitersSharedFLock, &waitersLockIDsFLock)



/**
 * Inode initialization. The preferred initializer. Used for loading an inode from disk
 */
FileInode::FileInode(std::string entryID, FileInodeStoreData* inodeDiskData,
   DirEntryType entryType, unsigned dentryFeatureFlags) : inodeDiskData(entryID, inodeDiskData)
{
   this->exclusiveTID     = 0;
   this->numSessionsRead  = 0;
   this->numSessionsWrite = 0;

   initFileInfoVec();

   this->dentryCompatData.entryType    = entryType;
   this->dentryCompatData.featureFlags = dentryFeatureFlags;
}

/**
 * Note: This constructor does not perform the full initialization, so use it for
 * metadata loading (or similar deserialization) only.
 *
 * Note: Don't forget to call initFileInfoVec() when using this (loadFromInodeFile() includes it).
 */
FileInode::FileInode()
{
   this->exclusiveTID     = 0;
   this->numSessionsRead  = 0;
   this->numSessionsWrite = 0;

   this->dentryCompatData.entryType    = DirEntryType_INVALID;
   this->dentryCompatData.featureFlags = 0;
}

/**
 * Requires: init'ed stripe pattern, modification and last access time secs
 */
void FileInode::initFileInfoVec()
{
   // create a fileInfo in the vector for each stripe node

   StripePattern* pattern = inodeDiskData.getStripePattern();
   size_t numTargets      = pattern->getStripeTargetIDs()->size();
   unsigned chunkSize     = pattern->getChunkSize();
   unsigned chunkSizeLog2 = MathTk::log2Int32(chunkSize);

   uint64_t stripeSetSize = chunkSize * numTargets;

   int64_t lastStripeSetSize; // =fileLength%stripeSetSize (remainder after stripeSetStart)
   int64_t stripeSetStart; // =fileLength-stripeSetSize
   int64_t fullLengthPerTarget; // =stripeSetStart/numTargets (without last stripe set remainder)

   StatData* statData = this->inodeDiskData.getInodeStatData();
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

   for(unsigned target=0; target < numTargets; target++) // iterate over all chunks / targets
   {
      int64_t targetFileLength = fullLengthPerTarget;

      if(remainingLastSetSize > 0)
         targetFileLength += BEEGFS_MIN(remainingLastSetSize, chunkSize);

      int64_t modificationTimeSecs = statData->getModificationTimeSecs();
      int64_t lastAccessTimeSecs   = statData->getLastAccessTimeSecs();

      uint64_t usedBlocks;
      if (statData->getIsSparseFile() )
         usedBlocks = statData->getTargetChunkBlocks(target);
      else
      {  // estimate the number of blocks by the file size
         usedBlocks = targetFileLength >> StatData::BLOCK_SHIFT;
      }

      DynamicFileAttribs dynAttribs(0, targetFileLength, usedBlocks, modificationTimeSecs,
         lastAccessTimeSecs);
      ChunkFileInfo fileInfo(chunkSize, chunkSizeLog2, dynAttribs);

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
            std::string("File: ") + getEntryIDUnlocked() ) );
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
            std::string("File: ") + getEntryIDUnlocked() ) );
      }
      else
         this->numSessionsWrite--;
   }

   // dyn attribs have been updated during close, so we save them here
   storeUpdatedInodeUnlocked(entryInfo);

   safeLock.unlock();
}


/**
 * Note: This version is compatible with sparse files.
 */
void FileInode::updateDynamicAttribs()
{
   this->inodeDiskData.inodeStatData.updateDynamicFileAttribs(this->fileInfoVec,
      this->inodeDiskData.getPattern() );
}

/*
 * Note: Current object state is used for the serialization
 */
void FileInode::serializeMetaData(Serializer& ser)
{
   // note: the total amount of serialized data may not be larger than META_SERBUF_SIZE

   // get latest dyn attrib values
   updateDynamicAttribs();

   NumNodeID ownerNodeID ; /* irrelevant here. The serialize will set it to ourselves for inlined
                            *  inodes */
   DentryStoreData dentryDiskData(this->inodeDiskData.getEntryID(),
      this->dentryCompatData.entryType, ownerNodeID, this->dentryCompatData.featureFlags);

   DiskMetaData diskMetaData(&dentryDiskData, &this->inodeDiskData);

   diskMetaData.serializeFileInode(ser);
}

/*
 * Note: Applies deserialized data directly to the current object
 */
void FileInode::deserializeMetaData(Deserializer& des)
{
   DentryStoreData dentryDiskData;
   DiskMetaData diskMetaData(&dentryDiskData, &this->inodeDiskData);

   diskMetaData.deserializeFileInode(des);
   if (!des.good())
      return;

   { // dentry compat data
     // entryType
      this->dentryCompatData.entryType = dentryDiskData.getDirEntryType();

      // (dentry) feature flags
      this->dentryCompatData.featureFlags = dentryDiskData.getDentryFeatureFlags();
   }
}


/**
 * Note: Wrapper/chooser for storeUpdatedMetaDataBufAsXAttr/Contents.
 * Note: Unlocked, caller must hold write lock.
 *
 * @param buf the serialized object state that is to be stored
 */
bool FileInode::storeUpdatedMetaDataBuf(char* buf, unsigned bufLen)
{
   App* app = Program::getApp();

   bool useXAttrs = app->getConfig()->getStoreUseExtendedAttribs();

   const Path* inodesPath =
      getIsBuddyMirroredUnlocked() ? app->getBuddyMirrorInodesPath() : app->getInodesPath();

   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodesPath->str(),
      inodeDiskData.getEntryID());

   bool result = useXAttrs
      ? storeUpdatedMetaDataBufAsXAttr(buf, bufLen, metaFilename)
      : storeUpdatedMetaDataBufAsContents(buf, bufLen, metaFilename);

   if (getIsBuddyMirroredUnlocked())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(metaFilename, MetaSyncFileType::Inode);

   return result;
}

/**
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool FileInode::storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen, std::string metaFilename)
{
   const char* logContext = "File (store updated xattr metadata)";

   // open file (create file if not already present)
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;
   int fd = open(metaFilename.c_str(), openFlags, 0644);

   if (unlikely(fd == -1))
   {
      LogContext(logContext).logErr("Unable to open/create inode metafile: " + metaFilename
         + ". " + "SysErr: " + System::getErrString());
      return false;
   }

   // write data to file

   int setRes = fsetxattr(fd, META_XATTR_NAME, buf, bufLen, 0);

   if(unlikely(setRes == -1) )
   { // error
      LogContext(logContext).logErr("Unable to write FileInode metadata update: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      close(fd);
      return false;
   }

   LOG_DEBUG(logContext, 4, "File inode update stored: " + this->inodeDiskData.getEntryID() );

   close(fd);
   return true;
}

/**
 * Stores the update to a sparate file first and then renames it.
 *
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool FileInode::storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen,
   std::string metaFilename)
{
   const char* logContext = "File (store updated inode)";

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

         return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen, metaFilename);
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

         return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen, metaFilename);
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

   LOG_DEBUG(logContext, 4, "Inode update stored: " + this->inodeDiskData.getEntryID() );

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
bool FileInode::storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen,
   std::string metaFilename)
{
   const char* logContext = "File (store updated inode in-place)";

   int fallocRes;
   ssize_t writeRes;
   int truncRes;

   // open file (create it, but not O_EXCL because a former update could have failed)
   int openFlags = O_CREAT|O_WRONLY;

   int fd = open(metaFilename.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      LogContext(logContext).logErr("Unable to open inode file: " + metaFilename +
         ". " + "SysErr: " + System::getErrString() );

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
            + StringTk::intToStr(bufLen)  + ") for inode update failed: " + metaFilename + ". " +
            "SysErr: " + System::getErrString(fallocRes) + " "
            "statRes: " + StringTk::intToStr(statRes) + " "
            "oldSize: " + StringTk::intToStr(statBuf.st_size));
         goto error_closefile;
      }
      else
      { // // XFS bug! We only return an error if statBuf.st_size < bufLen. Ingore fallocRes then
         LOG_DEBUG(logContext, Log_SPAM, "Ignoring kernel file system bug: "
            "posix_fallocate() failed for len < filesize");
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

   LOG_DEBUG(logContext, 4, "File inode update stored: " + this->inodeDiskData.getEntryID() );

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
bool FileInode::storeUpdatedInodeUnlocked(EntryInfo* entryInfo, StripePattern* updatedStripePattern)
{
   const char* logContext = "FileInode (store updated Inode)";
   bool saveRes;

   bool isInLined = this->isInlined;

   if (isInLined)
   {
      FhgfsOpsErr dentrySaveRes = storeUpdatedInlinedInodeUnlocked(entryInfo, updatedStripePattern);
      if (dentrySaveRes == FhgfsOpsErr_SUCCESS)
         return true;

      // dentrySaveRes != FhgfsOpsErr_SUCCESS
      std::string parentID = entryInfo->getParentEntryID();
      std::string entryID  = entryInfo->getEntryID();
      std::string fileName = entryInfo->getFileName();

      if (dentrySaveRes == FhgfsOpsErr_INODENOTINLINED)
      {
         /* dentrySaveRes == FhgfsOpsErr_INODENOTINLINED. Our internal inode information says the
          * inode is inlined, but on writing it we figure out it is not. As we we are holding a
          * write lock here, that never should have happened. So probably a locking bug, but not
          * critical here and we retry using the non-inlined way.
          */

         LogContext(logContext).log(Log_WARNING, std::string("Inode unexpectedly not inlined: ") +
            "parentID: "+ parentID +  " entryID: " + entryID + " fileName: " + fileName );
         this->isInlined = false;

      }
      else
      {
         LogContext(logContext).log(Log_WARNING, std::string("Failed to write inlined inode: ") +
            "parentID: "+ parentID +  " entryID: " + entryID + " fileName: " + fileName +
            " Error: " + boost::lexical_cast<std::string>(dentrySaveRes));
         #ifdef BEEGFS_DEBUG
            LogContext(logContext).logBacktrace();
         #endif

      }


      // it now falls through to the not-inlined handling, hopefully this is goint to work
   }

   // inode not inlined

   // change the stripe pattern here before serializing;

   if (unlikely(updatedStripePattern))
   {
      StripePattern* pattern = this->inodeDiskData.getPattern();
      if (!pattern->updateStripeTargetIDs(updatedStripePattern))
         LogContext(logContext).log(Log_WARNING, "Could not set requested new stripe pattern");
   }

   char buf[META_SERBUF_SIZE];
   Serializer ser(buf, sizeof(buf));

   serializeMetaData(ser);

   if (ser.good())
      saveRes = storeUpdatedMetaDataBuf(buf, ser.size());
   else
      saveRes = false;

   if (!saveRes && isInlined)
   {
      LogContext(logContext).log(Log_WARNING, std::string("Trying to write as non-inlined inode "
         "also failed.") );

   }

   return saveRes;
}

/**
 * Update an inode, which is inlined into a dentry
 */
FhgfsOpsErr FileInode::storeUpdatedInlinedInodeUnlocked(EntryInfo* entryInfo,
   StripePattern* updatedStripePattern)
{
   const char* logContext = "DirEntry (storeUpdatedInode)";
   App* app = Program::getApp();

   // get latest dyn attrib vals...
   updateDynamicAttribs();

   std::string parentEntryID = entryInfo->getParentEntryID();

   const Path* dentriesPath =
      entryInfo->getIsBuddyMirrored() ? app->getBuddyMirrorDentriesPath() : app->getDentriesPath();

   std::string dirEntryPath = MetaStorageTk::getMetaDirEntryPath(dentriesPath->str(),
      parentEntryID);

   FileInodeStoreData* inodeDiskData = this->getInodeDiskData();

   if (unlikely(updatedStripePattern))
   {
      // note: We do not set the complete stripe pattern here, but only the stripe target IDs
      if (! inodeDiskData->getPattern()->updateStripeTargetIDs(updatedStripePattern))
         LogContext(logContext).log(Log_WARNING, "Could not set new stripe target IDs.");
   }

   DirEntry dirEntry(entryInfo->getEntryType(), entryInfo->getFileName(),
      entryInfo->getEntryID(), entryInfo->getOwnerNodeID() );

   /* Note: As we are called from FileInode most data of this DirEntry are unknown and we need to
    *       load it from disk. */
   bool loadRes = dirEntry.loadFromID(dirEntryPath, entryInfo->getEntryID() );
   if (!loadRes)
      return FhgfsOpsErr_INTERNAL;

   FileInodeStoreData* entryInodeDiskData = dirEntry.getInodeStoreData();
   entryInodeDiskData->setFileInodeStoreData(inodeDiskData);

   FhgfsOpsErr retVal = dirEntry.storeUpdatedInode(dirEntryPath);

   return retVal;
}


bool FileInode::removeStoredMetaData(const std::string& id, bool isBuddyMirrored)
{
   const char* logContext = "FileInode (remove stored metadata)";

   App* app = Program::getApp();
   std::string inodeFilename = MetaStorageTk::getMetaInodePath(
         isBuddyMirrored
            ? app->getBuddyMirrorInodesPath()->str()
            : app->getInodesPath()->str(),
         id);

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

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addDeletion(inodeFilename, MetaSyncFileType::Inode);

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
      return loadFromFileXAttr(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored() );

   return loadFromFileContents(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored() );
}

/**
 * Note: Don't call this directly, use the wrapper loadFromInodeFile().
 * Note: This also calls initFileInfoVec()
 */
bool FileInode::loadFromFileXAttr(const std::string& id, bool isBuddyMirrored)
{
   const char* logContext = "File inode (load from xattr file)";
   App* app = Program::getApp();

   const Path* inodePath = isBuddyMirrored ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);

   bool retVal = false;

   char buf[META_SERBUF_SIZE];

   ssize_t getRes = getxattr(metaFilename.c_str(), META_XATTR_NAME, buf, META_SERBUF_SIZE);
   if(getRes > 0)
   { // we got something => deserialize it
      Deserializer des(buf, getRes);
      deserializeMetaData(des);

      if(unlikely(!des.good()))
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize metadata in file: " + metaFilename);
         goto error_exit;
      }

      // deserialization successful => init dyn attribs

      initFileInfoVec(); /* note: this can only be done after the stripePattern
         has been initialized, that's why we do it here at this "unusual" place. */

      retVal = true;
   }
   else
   if( (getRes == -1) && (errno == ENOENT) )
   { // file not exists
      LOG_DEBUG_CONTEXT(LogContext(logContext), Log_DEBUG, "Inode file not exists: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );
   }
   else
   { // unhandled error
      LogContext(logContext).logErr("Unable to open/read inode file: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );
   }


error_exit:

   return retVal;
}

/**
 * Note: Don't call this directly, use the wrapper loadFromInodeFile().
 * Note: This also calls initFileInfoVec()
 */
bool FileInode::loadFromFileContents(const std::string& id, bool isBuddyMirrored)
{
   const char* logContext = "File inode (load from file)";
   App* app = Program::getApp();

   const Path* inodePath = isBuddyMirrored ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
   bool retVal = false;

   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(metaFilename.c_str(), openFlags, 0);
   if(fd == -1)
   { // open failed
      if(errno != ENOENT)
         LogContext(logContext).logErr("Unable to open inode file: " + metaFilename + ". " +
            "SysErr: " + System::getErrString() );

      return false;
   }

   char buf[META_SERBUF_SIZE];
   int readRes = read(fd, buf, META_SERBUF_SIZE);
   if(readRes <= 0)
   { // reading failed
      LogContext(logContext).logErr("Unable to read inode file: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   {
      Deserializer des(buf, readRes);
      deserializeMetaData(des);
      if(!des.good())
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize inode in file: " + metaFilename);
      }
      else
      { // deserialization successful => init dyn attribs
         initFileInfoVec(); // note: this can only be done after the stripePattern
            // has been initialized, that's why we do it here at this "unusual" place

         retVal = true;
      }
   }

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

   if (entryInfo->getIsInlined() )
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
 * Inode from inode file (inode is not inlined)
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

   newInode->setIsInlinedUnlocked(false);

   return newInode;
}


/**
 * Inode from dir-entry with inlined inode.
 *
 * Note: Do not call directly, but use FileInode::createFromEntryInfo()
 */
FileInode* FileInode::createFromInlinedInode(EntryInfo* entryInfo)
{
   App* app = Program::getApp();

   std::string parentEntryID = entryInfo->getParentEntryID();

   const Path* dentryPath =
      entryInfo->getIsBuddyMirrored() ? app->getBuddyMirrorDentriesPath() : app->getDentriesPath();

   std::string dirEntryPath = MetaStorageTk::getMetaDirEntryPath(dentryPath->str(),
      parentEntryID);

   DirEntry dirEntry(entryInfo->getEntryType(), entryInfo->getFileName(),
      entryInfo->getEntryID(), entryInfo->getOwnerNodeID() );

   FileInode* newInode = dirEntry.createInodeByID(dirEntryPath, entryInfo);

   if (newInode)
      newInode->setIsInlinedUnlocked(true);

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
   StatData* statData = this->inodeDiskData.getInodeStatData();
   SettableFileAttribs oldAttribs = *(statData->getSettableFileAttribs() );

   statData->setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);

   if(validAttribs)
   {
      // apply new attribs wrt flags...

      if(validAttribs & SETATTR_CHANGE_MODE)
         statData->setMode(attribs->mode);

      if(validAttribs & SETATTR_CHANGE_MODIFICATIONTIME)
      {
         /* only static value update required for storeUpdatedInodeUnlocked() */
         statData->setModificationTimeSecs(attribs->modificationTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_LASTACCESSTIME)
      {
         /* only static value update required for storeUpdatedInodeUnlocked() */
         statData->setLastAccessTimeSecs(attribs->lastAccessTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_USERID)
      {
         statData->setUserID(attribs->userID);

         if ((attribs->userID != this->inodeDiskData.getOrigUID() ) &&
            (this->inodeDiskData.getOrigFeature() == FileInodeOrigFeature_TRUE) )
            addFeatureFlagUnlocked(FILEINODE_FEATURE_HAS_ORIG_UID);
      }

      if(validAttribs & SETATTR_CHANGE_GROUPID)
         statData->setGroupID(attribs->groupID);
   }

   bool storeRes = storeUpdatedInodeUnlocked(entryInfo); // store on disk
   if(!storeRes)
   { // failed to update metadata on disk => restore old values
      statData->setSettableFileAttribs(oldAttribs);

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
 * General wrapper for append lock and unlock operations.
 *
 * Append supports exclusive locking only, no shared locks.
 *
 * Note: Unlocks are always immediately granted (=> they always return "true").
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
std::pair<bool, LockEntryNotifyList> FileInode::flockAppend(EntryLockDetails& lockDetails)
{
   FILEINODE_APPEND_LOCK_QUEUES_CONTAINER(lockQs);

   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   return flockEntryUnlocked(lockDetails, &lockQs);
}

/**
 * General wrapper for flock lock and unlock operations.
 *
 * Note: Unlocks are always immediately granted (=> they always return "true").
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
std::pair<bool, LockEntryNotifyList> FileInode::flockEntry(EntryLockDetails& lockDetails)
{
   FILEINODE_ENTRY_LOCK_QUEUES_CONTAINER(lockQs);

   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   return flockEntryUnlocked(lockDetails, &lockQs);
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
std::pair<bool, LockEntryNotifyList> FileInode::flockEntryUnlocked(EntryLockDetails& lockDetails,
   EntryLockQueuesContainer* lockQs)
{
   bool tryNextWaiters = false;
   bool immediatelyGranted = false; // return value

   if(lockDetails.isCancel() )
   {
      // C A N C E L request

      /* note: this is typically used when a client closes a file, so we remove all granted and
         pending locks for the given handle here */

      if(flockEntryCancelByHandle(lockDetails, lockQs) )
         tryNextWaiters = true;

      immediatelyGranted = true;
   }
   else
   if(lockDetails.isUnlock() )
   {
      // U N L O C K request

      tryNextWaiters = flockEntryUnlock(lockDetails, lockQs);
      immediatelyGranted = true;
   }
   else
   {
      // L O C K request

      // check waiters to filter duplicate requests

      StringSetIter iterWaiters = lockQs->waitersLockIDs->find(lockDetails.lockAckID);
      if(iterWaiters != lockQs->waitersLockIDs->end() )
         return {false, {}}; // re-request from waiter, but still in the queue => keep on waiting

      // not in waiters queue => is it granted already?

      bool isGrantedAlready = flockEntryIsGranted(lockDetails, lockQs);
      if(isGrantedAlready)
         return {true, {}}; // request was granted already

      // not waiting, not granted => we have a new request

      bool hasConflicts = flockEntryCheckConflicts(lockDetails, lockQs, NULL);

      if(!hasConflicts || lockDetails.allowsWaiting() )
         tryNextWaiters = flockEntryUnlock(lockDetails, lockQs); // unlock (for lock up-/downgrades)

      if(lockDetails.isShared() )
      {
         // S H A R E D lock request

         if(!hasConflicts)
         { // no confictors for this lock => can be immediately granted
            flockEntryShared(lockDetails, lockQs);
            immediatelyGranted = true;
         }
         else
         if(lockDetails.allowsWaiting() )
         { // we have conflictors and locker wants to wait
            lockQs->waitersSharedLock->push_back(lockDetails);
            lockQs->waitersLockIDs->insert(lockDetails.lockAckID);
         }
      }
      else
      {
         // E X C L U S I V E lock request

         if(!hasConflicts)
         { // no confictors for this lock => can be immediately granted
            flockEntryExclusive(lockDetails, lockQs);
            immediatelyGranted = true;
         }
         else
         if(lockDetails.allowsWaiting() )
         { // we have conflictors and locker wants to wait
            lockQs->waitersExclLock->push_back(lockDetails);
            lockQs->waitersLockIDs->insert(lockDetails.lockAckID);
         }
      }
   }

   if (tryNextWaiters)
      return {immediatelyGranted, flockEntryTryNextWaiters(lockQs)};

   return {immediatelyGranted, {}};
}

/**
 * Remove all waiters from the queues.
 */
void FileInode::flockAppendCancelAllWaiters()
{
   FILEINODE_APPEND_LOCK_QUEUES_CONTAINER(lockQs);

   flockEntryGenericCancelAllWaiters(&lockQs);
}

/**
 * Remove all waiters from the queues.
 */
void FileInode::flockEntryCancelAllWaiters()
{
   FILEINODE_ENTRY_LOCK_QUEUES_CONTAINER(lockQs);

   flockEntryGenericCancelAllWaiters(&lockQs);
}

/**
 * Remove all waiters from the queues.
 *
 * Generic version shared by append and flock locking.
 */
void FileInode::flockEntryGenericCancelAllWaiters(EntryLockQueuesContainer* lockQs)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   lockQs->waitersLockIDs->clear();
   lockQs->waitersExclLock->clear();
   lockQs->waitersSharedLock->clear();
}


/**
 * Unlock all locks and wait entries of the given clientID.
 */
LockEntryNotifyList FileInode::flockAppendCancelByClientID(NumNodeID clientID)
{
   FILEINODE_APPEND_LOCK_QUEUES_CONTAINER(lockQs);

   return flockEntryGenericCancelByClientID(clientID, &lockQs);
}

/**
 * Unlock all locks and wait entries of the given clientID.
 */
LockEntryNotifyList FileInode::flockEntryCancelByClientID(NumNodeID clientID)
{
   FILEINODE_ENTRY_LOCK_QUEUES_CONTAINER(lockQs);

   return flockEntryGenericCancelByClientID(clientID, &lockQs);
}

/**
 * Unlock all locks and wait entries of the given clientID.
 *
 * Generic version shared by append and flock locking.
 */
LockEntryNotifyList FileInode::flockEntryGenericCancelByClientID(NumNodeID clientNumID,
   EntryLockQueuesContainer* lockQs)
{
   /* note: this code is in many aspects similar to flockEntryCancelByHandle(), so if you change
    * something here, you probably want to change it there, too. */

   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   bool tryNextWaiters = false;

   // exclusive lock

   if(lockQs->exclLock->isSet() && (lockQs->exclLock->clientNumID == clientNumID) )
   {
      *lockQs->exclLock = {};
      tryNextWaiters = true;
   }

   // shared locks

   for(EntryLockDetailsSetIter iter = lockQs->sharedLocks->begin();
       iter != lockQs->sharedLocks->end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientNumID == clientNumID)
      {
         EntryLockDetailsSetIter iterNext = iter;
         iterNext++;

         lockQs->sharedLocks->erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters exlusive

   for(EntryLockDetailsListIter iter = lockQs->waitersExclLock->begin();
       iter != lockQs->waitersExclLock->end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientNumID == clientNumID)
      {
         lockQs->waitersLockIDs->erase(iter->lockAckID);
         iter = lockQs->waitersExclLock->erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters shared

   for(EntryLockDetailsListIter iter = lockQs->waitersSharedLock->begin();
       iter != lockQs->waitersSharedLock->end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientNumID == clientNumID)
      {
         lockQs->waitersLockIDs->erase(iter->lockAckID);
         iter = lockQs->waitersSharedLock->erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   if (tryNextWaiters)
      return flockEntryTryNextWaiters(lockQs);

   return {};
}

/**
 * Remove all granted and pending locks that match the given handle.
 * (This is typically called by clients during file close.)
 *
 * Note: unlocked, so hold the mutex when calling this.
 *
 * @return true if locks were removed and next waiters should be tried.
 */
bool FileInode::flockEntryCancelByHandle(EntryLockDetails& lockDetails,
   EntryLockQueuesContainer* lockQs)
{
   /* note: this code is in many aspects similar to flockEntryCancelByClientID(), so if you change
    * something here, you probably want to change it there, too. */


   bool tryNextWaiters = false;

   // exclusive lock

   if(lockQs->exclLock->isSet() && lockDetails.equalsHandle(*lockQs->exclLock) )
   {
      *lockQs->exclLock = {};
      tryNextWaiters = true;
   }

   // shared locks

   for(EntryLockDetailsSetIter iter = lockQs->sharedLocks->begin();
       iter != lockQs->sharedLocks->end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         EntryLockDetailsSetIter iterNext = iter;
         iterNext++;

         lockQs->sharedLocks->erase(iter);

         iter = iterNext;
         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters exlusive

   for(EntryLockDetailsListIter iter = lockQs->waitersExclLock->begin();
       iter != lockQs->waitersExclLock->end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         lockQs->waitersLockIDs->erase(iter->lockAckID);
         iter = lockQs->waitersExclLock->erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   // waiters shared

   for(EntryLockDetailsListIter iter = lockQs->waitersSharedLock->begin();
       iter != lockQs->waitersSharedLock->end();
       /* iter inc'ed inside loop */ )
   {
      if(lockDetails.equalsHandle(*iter) )
      {
         lockQs->waitersLockIDs->erase(iter->lockAckID);
         iter = lockQs->waitersSharedLock->erase(iter);

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
bool FileInode::flockEntryCheckConflicts(EntryLockDetails& lockDetails,
   EntryLockQueuesContainer* lockQs, EntryLockDetails* outConflictor)
{
   // note: we also check waiting writers here, because we have writer preference and so we don't
      // want to grant access for a new reader if we have a waiting writer


   // check conflicting exclusive lock (for shared & eclusive requests)

   if(lockQs->exclLock->isSet() && !lockQs->exclLock->equalsHandle(lockDetails) )
   {
      SAFE_ASSIGN(outConflictor, *lockQs->exclLock);
      return true;
   }

   // no exclusive lock exists

   if(lockDetails.isExclusive() )
   { // exclusive lock request: check conflicting shared lock

      for(EntryLockDetailsSetCIter iterShared = lockQs->sharedLocks->begin();
          iterShared != lockQs->sharedLocks->end();
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

      if(!lockQs->waitersExclLock->empty() )
      {
         SAFE_ASSIGN(outConflictor, *lockQs->waitersExclLock->begin() );
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
bool FileInode::flockEntryIsGranted(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs)
{
   if(lockDetails.isExclusive() )
   {
      if(lockQs->exclLock->equalsHandle(lockDetails) )
      { // was an exclusive lock
         return true;
      }
   }
   else
   if(lockDetails.isShared() )
   {
      EntryLockDetailsSetIter iterShared = lockQs->sharedLocks->find(lockDetails);
      if(iterShared != lockQs->sharedLocks->end() )
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
bool FileInode::flockEntryUnlock(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs)
{
   if(lockQs->exclLock->equalsHandle(lockDetails) )
   { // was an exclusive lock
      *lockQs->exclLock = {};
      return true;
   }

   EntryLockDetailsSetIter iterShared = lockQs->sharedLocks->find(lockDetails);
   if(iterShared != lockQs->sharedLocks->end() )
   { // was a shared lock
      lockQs->sharedLocks->erase(iterShared);

      return true;
   }

   return false;
}


/**
 * Note: We assume that unlock() has been called before, so we don't check for up-/downgrades or
 * duplicates.
 * Note: unlocked, so hold the mutex when calling this
 */
void FileInode::flockEntryShared(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs)
{
   lockQs->sharedLocks->insert(lockDetails);
}

/**
 * Note: We assume that unlock() has been called before, so we don't check for up-/downgrades or
 * duplicates.
 * Note: unlocked, so hold the mutex when calling this
 */
void FileInode::flockEntryExclusive(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs)
{
   *lockQs->exclLock = lockDetails;
}


/**
 * Remove next requests from waiters queue and try to grant it - until we reach an entry that
 * cannot be granted immediately.
 *
 * Note: We assume that duplicate waiters and duplicate granted locks (up-/downgrades) have been
 * removed before a lock request is enqueued, so we don't check for that.
 *
 * Note: FileInode must be already write-locked by the caller!
 */
LockEntryNotifyList FileInode::flockEntryTryNextWaiters(EntryLockQueuesContainer* lockQs)
{
   /* note: we have writer preference, so we don't grant any new readers while we have waiting
      writers */

   if(lockQs->exclLock->isSet() )
      return {}; // eclusive lock => there's nothing we can do right now

   // no exclusive lock set

   if(!lockQs->waitersSharedLock->empty() && lockQs->waitersExclLock->empty() )
   { // shared locks waiting and no exclusive locks waiting => grant all

      LockEntryNotifyList notifyList;

      while(!lockQs->waitersSharedLock->empty() )
      {
         flockEntryShared(*lockQs->waitersSharedLock->begin(), lockQs);

         notifyList.push_back(*lockQs->waitersSharedLock->begin() );

         lockQs->waitersLockIDs->erase(lockQs->waitersSharedLock->begin()->lockAckID);
         lockQs->waitersSharedLock->pop_front();
      }

      return notifyList;
   }

   // no exclusive and no shared locks set => we can grant an exclusive lock

   if(!lockQs->waitersExclLock->empty() )
   { // exclusive locks waiting => grant first one of them
      flockEntryExclusive(*lockQs->waitersExclLock->begin(), lockQs);

      LockEntryNotifyList notifyList;
      notifyList.push_back(*lockQs->waitersExclLock->begin() );

      lockQs->waitersLockIDs->erase(lockQs->waitersExclLock->begin()->lockAckID);
      lockQs->waitersExclLock->pop_front();

      return notifyList;
   }

   return {};
}

/**
 * Generate a complete locking status overview (all granted and waiters) as human-readable string.
 */
std::string FileInode::flockAppendGetAllAsStr()
{
   FILEINODE_APPEND_LOCK_QUEUES_CONTAINER(lockQs);

   return flockEntryGenericGetAllAsStr(&lockQs);
}

/**
 * Generate a complete locking status overview (all granted and waiters) as human-readable string.
 */
std::string FileInode::flockEntryGetAllAsStr()
{
   FILEINODE_ENTRY_LOCK_QUEUES_CONTAINER(lockQs);

   return flockEntryGenericGetAllAsStr(&lockQs);
}

/**
 * Generate a complete locking status overview (all granted and waiters) as human-readable string.
 *
 * Generic version shared by append and flock locking.
 */
std::string FileInode::flockEntryGenericGetAllAsStr(EntryLockQueuesContainer* lockQs)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   std::ostringstream outStream;

   outStream << "Exclusive" << std::endl;
   outStream << "=========" << std::endl;
   if(lockQs->exclLock->isSet() )
      outStream << lockQs->exclLock->toString() << std::endl;

   outStream << std::endl;

   outStream << "Shared" << std::endl;
   outStream << "=========" << std::endl;
   for(EntryLockDetailsSetCIter iter = lockQs->sharedLocks->begin();
       iter != lockQs->sharedLocks->end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Exclusive Waiters" << std::endl;
   outStream << "=========" << std::endl;
   for(EntryLockDetailsListCIter iter = lockQs->waitersExclLock->begin();
       iter != lockQs->waitersExclLock->end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Shared Waiters" << std::endl;
   outStream << "=========" << std::endl;
   for(EntryLockDetailsListCIter iter = lockQs->waitersSharedLock->begin();
       iter != lockQs->waitersSharedLock->end();
       iter++)
   {
      outStream << iter->toString() << std::endl;
   }

   outStream << std::endl;

   outStream << "Waiters lockIDs" << std::endl;
   outStream << "=========" << std::endl;
   for(StringSetCIter iter = lockQs->waitersLockIDs->begin();
       iter != lockQs->waitersLockIDs->end();
       iter++)
   {
      outStream << *iter << std::endl;
   }

   outStream << std::endl;

   return outStream.str();
}

/**
 * General wrapper for flock lock and unlock operations.
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
std::pair<bool, LockRangeNotifyList> FileInode::flockRange(RangeLockDetails& lockDetails)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   return flockRangeUnlocked(lockDetails);
}

/**
 * General wrapper for flock lock and unlock operations.
 *
 * Note: Unlocked, so caller must hold the write lock.
 *
 * @return true if operation succeeded immediately; false if registered for waiting (or failed in
 * case of NOWAIT-flag)
 */
std::pair<bool, LockRangeNotifyList> FileInode::flockRangeUnlocked(RangeLockDetails& lockDetails)
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
         return {false, {}}; // re-request from waiter, but still in the queue => keep on waiting

      // not in waiters queue => is it granted already?

      bool isGrantedAlready = flockRangeIsGranted(lockDetails);
      if(isGrantedAlready)
         return {true, {}}; // request was granted already

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

   if (tryNextWaiters)
      return {immediatelyGranted, flockRangeTryNextWaiters()};

   return {immediatelyGranted, {}};
}

/**
 * Remove all waiters from the queues.
 */
void FileInode::flockRangeCancelAllWaiters()
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   waitersLockIDsRangeFLock.clear();
   waitersExclRangeFLock.clear();
   waitersSharedRangeFLock.clear();
}

/**
 * Unlock all locks and wait entries of the given clientID.
 */
LockRangeNotifyList FileInode::flockRangeCancelByClientID(NumNodeID clientNumID)
{
   /* note: this code is in many aspects similar to flockRangeCancelByHandle(), so if you change
    * something here, you probably want to change it there, too. */

   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   bool tryNextWaiters = false;

   // exclusive locks

   for(RangeLockExclSetIter iter = exclRangeFLocks.begin();
       iter != exclRangeFLocks.end();
       /* iter inc'ed inside loop */ )
   {
      if(iter->clientNumID == clientNumID)
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
      if(iter->clientNumID == clientNumID)
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
      if(iter->clientNumID == clientNumID)
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
      if(iter->clientNumID == clientNumID)
      {
         waitersLockIDsRangeFLock.erase(iter->lockAckID);
         iter = waitersSharedRangeFLock.erase(iter);

         tryNextWaiters = true;
         continue;
      }

      iter++;
   }

   if(tryNextWaiters)
      return flockRangeTryNextWaiters();

   return {};
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
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   return flockRangeCheckConflicts(lockDetails, outConflictor);
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
LockRangeNotifyList FileInode::flockRangeTryNextWaiters()
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

   return notifyList;
}


/**
 * Generate a complete locking status overview (all granted and waiters) as human-readable string.
 */
std::string FileInode::flockRangeGetAllAsStr()
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

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

   return outStream.str();
}

/**
 * Increase/decreas the link count of this inode
 */
bool FileInode::incDecNumHardLinks(EntryInfo* entryInfo, int value)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   incDecNumHardlinksUnpersistentUnlocked(value);

   // update ctime
   StatData* statData = this->inodeDiskData.getInodeStatData();
   statData->setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);

   bool retVal = storeUpdatedInodeUnlocked(entryInfo); // store on disk
   if(!retVal)
   {  // failed to update metadata on disk => restore old values
      incDecNumHardlinksUnpersistentUnlocked(-value);
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}

bool FileInode::operator==(const FileInode& other) const
{
   return inodeDiskData == other.inodeDiskData
      && fileInfoVec == other.fileInfoVec
      && exclusiveTID == other.exclusiveTID
      && numSessionsRead == other.numSessionsRead
      && numSessionsWrite == other.numSessionsWrite
      && exclAppendLock == other.exclAppendLock
      && waitersExclAppendLock == other.waitersExclAppendLock
      && waitersLockIDsAppendLock == other.waitersLockIDsAppendLock
      && exclFLock == other.exclFLock
      && sharedFLocks == other.sharedFLocks
      && waitersExclFLock == other.waitersExclFLock
      && waitersSharedFLock == other.waitersSharedFLock
      && waitersLockIDsFLock == other.waitersLockIDsFLock
      && exclRangeFLocks == other.exclRangeFLocks
      && sharedRangeFLocks == other.sharedRangeFLocks
      && waitersExclRangeFLock == other.waitersExclRangeFLock
      && waitersSharedRangeFLock == other.waitersSharedRangeFLock
      && waitersLockIDsRangeFLock == other.waitersLockIDsRangeFLock
      && dentryCompatData == other.dentryCompatData
      && numParentRefs.read() == other.numParentRefs.read()
      && referenceParentID == other.referenceParentID
      && isInlined == other.isInlined;
}

std::pair<FhgfsOpsErr, StringVector> FileInode::listXAttr()
{
   BEEGFS_BUG_ON_DEBUG(isInlined, "inlined file inode cannot access its own xattrs");

   const Path* inodesPath = getIsBuddyMirroredUnlocked()
      ? Program::getApp()->getBuddyMirrorInodesPath()
      : Program::getApp()->getInodesPath();

   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodesPath->str(),
      inodeDiskData.getEntryID());

   return XAttrTk::listUserXAttrs(metaFilename);
}

std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> FileInode::getXAttr(
   const std::string& xAttrName, size_t maxSize)
{
   BEEGFS_BUG_ON_DEBUG(isInlined, "inlined file inode cannot access its own xattrs");

   const Path* inodesPath = getIsBuddyMirroredUnlocked()
      ? Program::getApp()->getBuddyMirrorInodesPath()
      : Program::getApp()->getInodesPath();

   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodesPath->str(),
      inodeDiskData.getEntryID());

   return XAttrTk::getUserXAttr(metaFilename, xAttrName, maxSize);
}

FhgfsOpsErr FileInode::removeXAttr(EntryInfo* entryInfo, const std::string& xAttrName)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   BEEGFS_BUG_ON_DEBUG(isInlined, "inlined file inode cannot access its own xattrs");

   const Path* inodesPath = getIsBuddyMirroredUnlocked()
      ? Program::getApp()->getBuddyMirrorInodesPath()
      : Program::getApp()->getInodesPath();

   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodesPath->str(),
      inodeDiskData.getEntryID());

   FhgfsOpsErr result = XAttrTk::removeUserXAttr(metaFilename, xAttrName);

   if (result == FhgfsOpsErr_SUCCESS)
   {
      inodeDiskData.inodeStatData.setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);
      storeUpdatedInodeUnlocked(entryInfo, nullptr);
   }

   // FIXME: should resync only this xattr ON THE INODE
   if (getIsBuddyMirroredUnlocked())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(metaFilename, MetaSyncFileType::Inode);

   return result;
}

FhgfsOpsErr FileInode::setXAttr(EntryInfo* entryInfo, const std::string& xAttrName,
   const CharVector& xAttrValue, int flags)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   BEEGFS_BUG_ON_DEBUG(isInlined, "inlined file inode cannot access its own xattrs");

   const Path* inodesPath = getIsBuddyMirroredUnlocked()
      ? Program::getApp()->getBuddyMirrorInodesPath()
      : Program::getApp()->getInodesPath();

   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodesPath->str(),
      inodeDiskData.getEntryID());

   FhgfsOpsErr result = XAttrTk::setUserXAttr(metaFilename, xAttrName, &xAttrValue[0],
         xAttrValue.size(), flags);

   if (result == FhgfsOpsErr_SUCCESS)
   {
      inodeDiskData.inodeStatData.setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);
      storeUpdatedInodeUnlocked(entryInfo, nullptr);
   }

   // FIXME: should resync only this xattr ON THE INODE
   if (getIsBuddyMirroredUnlocked())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(metaFilename, MetaSyncFileType::Inode);

   return result;
}

void FileInode::initLocksRandomForSerializationTests()
{
   Random rand;

   this->exclusiveTID = rand.getNextInt();
   this->numSessionsRead = rand.getNextInt();
   this->numSessionsWrite = rand.getNextInt();


   this->exclAppendLock.initRandomForSerializationTests();

   int max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      EntryLockDetails lock;
      lock.initRandomForSerializationTests();
      this->waitersExclAppendLock.push_back(lock);
   }

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      std::string id;
      StringTk::genRandomAlphaNumericString(id, rand.getNextInRange(2, 30) );
      this->waitersLockIDsAppendLock.insert(id);
   }


   this->exclFLock.initRandomForSerializationTests();

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      EntryLockDetails lock;
      lock.initRandomForSerializationTests();
      this->sharedFLocks.insert(lock);
   }

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      EntryLockDetails lock;
      lock.initRandomForSerializationTests();
      this->waitersExclFLock.push_back(lock);
   }

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      EntryLockDetails lock;
      lock.initRandomForSerializationTests();
      this->waitersSharedFLock.push_back(lock);
   }


   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      std::string id;
      StringTk::genRandomAlphaNumericString(id, rand.getNextInRange(2, 30) );
      this->waitersLockIDsFLock.insert(id);
   }


   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      RangeLockDetails lock;
      lock.initRandomForSerializationTests();
      this->exclRangeFLocks.insert(lock);
   }

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      RangeLockDetails lock;
      lock.initRandomForSerializationTests();
      this->sharedRangeFLocks.insert(lock);
   }

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      RangeLockDetails lock;
      lock.initRandomForSerializationTests();
      this->waitersExclRangeFLock.push_back(lock);
   }

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      RangeLockDetails lock;
      lock.initRandomForSerializationTests();
      this->waitersSharedRangeFLock.push_back(lock);
   }

   max = rand.getNextInRange(0, 1024);
   for(int i = 0; i < max; i++)
   {
      std::string id;
      StringTk::genRandomAlphaNumericString(id, rand.getNextInRange(2, 30) );
      this->waitersLockIDsFLock.insert(id);
   }


   StringTk::genRandomAlphaNumericString(this->referenceParentID, rand.getNextInRange(2, 30) );
   this->numParentRefs.set(rand.getNextInt() );
}
