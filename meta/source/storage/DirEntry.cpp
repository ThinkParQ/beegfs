#include <common/toolkit/serialization/Serialization.h>
#include <components/buddyresyncer/BuddyResyncer.h>
#include <program/Program.h>
#include "MetadataEx.h"
#include "DirEntry.h"

#include <sys/xattr.h>

/*
 * Store the dirEntryID file. This is a normal dirEntry (with inlined inode),
 * but the file name is the entryID.
 *
 * @param logContext
 * @param idPath - path to the idFile, including the file name
 */
FhgfsOpsErr DirEntry::storeInitialDirEntryID(const char* logContext, const std::string& idPath)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   char buf[DIRENTRY_SERBUF_SIZE];
   Serializer ser(buf, sizeof(buf));
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   // create file

   /* note: if we ever think about switching to a rename-based version here, we must keep very
     long user file names in mind, which might lead to problems if we add an extension to the
     temporary file name. */

   int openFlags = O_CREAT|O_EXCL|O_WRONLY;

   int fd = open(idPath.c_str(), openFlags, 0644);
   if (unlikely (fd == -1) ) // this is our ID file, failing to create it is very unlikely
   { // error
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
      {
         /* EEXIST never should happen, as our ID is supposed to be unique, but there rare cases
          * as for the upgrade tool */
         retVal = FhgfsOpsErr_EXISTS;
         #ifdef BEEGFS_DEBUG
            LogContext(logContext).logBacktrace();
         #endif
      }
      else
      {
         retVal = FhgfsOpsErr_INTERNAL;
      }

      return retVal;
   }

   // serialize (to new buf)
   serializeDentry(ser);
   if (!ser.good())
   {
      LogContext(logContext).logErr("Dentry too large: " + idPath + ".");
      retVal = FhgfsOpsErr_INTERNAL;
   }

   // write buf to file

   if(useXAttrs)
   { // extended attribute
      int setRes = fsetxattr(fd, META_XATTR_NAME, buf, ser.size(), 0);

      if(unlikely(setRes == -1) )
      { // error
         LogContext(logContext).logErr("Unable to store dentry xattr metadata: " + idPath + ". " +
            "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;
         goto error_closefile;
      }
   }
   else
   { // normal file content
      ssize_t writeRes = write(fd, buf, ser.size());

      if(unlikely(writeRes != (ssize_t)ser.size()))
      { // error
         LogContext(logContext).logErr("Unable to store dentry metadata: " + idPath + ". " +
            "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;
         goto error_closefile;
      }
   }

   close(fd);

   return retVal;

   // error compensation
error_closefile:
   close(fd);

   int unlinkRes = unlink(idPath.c_str() );
   if (unlikely(unlinkRes && errno != ENOENT) )
   {
      LogContext(logContext).logErr("Creating the dentry-by-name file failed and"
         "now also deleting the dentry-by-id file fails: " + idPath);
   }

   return retVal;
}

/**
 * Store the dirEntry as file name
 */
FhgfsOpsErr DirEntry::storeInitialDirEntryName(const char* logContext, const std::string& idPath,
   const std::string& namePath, bool isNonInlinedInode)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   int linkRes = link(idPath.c_str(),  namePath.c_str() );
   if (linkRes)
   {  /* Creating the dirEntry-by-name failed, most likely this is EEXIST.
       * In principle it also might be possible there is an invalid dentry-by-name file,
       * however, we already want to delete those during lookup calls now. So invalid
       * entries are supposed to be very very unlikely and so no self-healing code is
       * implemented here. */

      if (likely(errno == EEXIST) )
         retVal = FhgfsOpsErr_EXISTS;
      else
      {
         LogContext(logContext).logErr("Creating the dentry-by-name file failed: Path: " +
            namePath + " SysErr: " + System::getErrString() );

         retVal = FhgfsOpsErr_INTERNAL;
      }

      int unlinkRes = unlink(idPath.c_str() );
      if (unlikely(unlinkRes) )
      {
         LogContext(logContext).logErr("Creating the dentry-by-name file failed and"
            "now also deleting the dentry-by-id file fails: " + idPath);
      }

      return retVal;
   }

   if (isNonInlinedInode)
   {
      // unlink the dentry-by-id file - we don't need it for dirs (or non-inlined inodes in general)
      int unlinkRes = unlink(idPath.c_str() );
      if (unlikely(unlinkRes) )
      {
         LogContext(logContext).logErr("Failed to unlink the (dir) dentry-by-id file "+ idPath +
            " SysErr: " + System::getErrString() );
      }
   }

   LOG_DEBUG(logContext, 4, "Initial dirEntry stored: " + namePath);

   return retVal;
}

/**
 * Note: Wrapper/chooser for storeUpdatedDirEntryBufAsXAttr/Contents.
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirEntry::storeUpdatedDirEntryBuf(const std::string& idStorePath, char* buf, unsigned bufLen)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   bool result = useXAttrs
      ? storeUpdatedDirEntryBufAsXAttr(idStorePath, buf, bufLen)
      : storeUpdatedDirEntryBufAsContents(idStorePath, buf, bufLen);

   return result;
}

/**
 * Note: Don't call this directly, use the wrapper storeUpdatedDirEntryBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirEntry::storeUpdatedDirEntryBufAsXAttr(const std::string& idStorePath,
   char* buf, unsigned bufLen)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(store updated xattr metadata)";

   // write data to file

   int setRes = setxattr(idStorePath.c_str(), META_XATTR_NAME, buf, bufLen, 0);

   if(unlikely(setRes == -1) )
   { // error
      LogContext(logContext).logErr("Unable to write dentry update: " +
         idStorePath + ". " + "SysErr: " + System::getErrString() );

      return false;
   }


   LOG_DEBUG(logContext, 4, "Dentry update stored: " + idStorePath);

   return true;
}

/**
 * Stores the update directly to the current metadata file (instead of creating a separate file
 * first and renaming it).
 *
 * Note: Don't call this directly, use the wrapper storeUpdatedDirEntryBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirEntry::storeUpdatedDirEntryBufAsContents(const std::string& idStorePath, char* buf,
   unsigned bufLen)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(store updated metadata in-place)";

   int fallocRes;
   ssize_t writeRes;
   int truncRes;

   // open file (create it, but not O_EXCL because a former update could have failed)

   int openFlags = O_CREAT|O_WRONLY;

   int fd = open(idStorePath.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      LogContext(logContext).logErr("Unable to create dentry metadata update file: " +
         idStorePath + ". " + "SysErr: " + System::getErrString() );

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
            + System::getErrString(errno) );
         goto error_closefile;
      }

      if (statBuf.st_size < bufLen)
      {
         LogContext(logContext).log(Log_WARNING, "File space allocation ("
            + StringTk::intToStr(bufLen)  + ") for metadata update failed: " + idStorePath + ". " +
            "SysErr: " + System::getErrString(fallocRes) + " "
            "statRes: " + StringTk::intToStr(statRes) + " "
            "oldSize: " + StringTk::intToStr(statBuf.st_size) );
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
         + StringTk::intToStr(bufLen)  + ") for metadata update failed: " + idStorePath + ". " +
         "SysErr: " + System::getErrString(fallocRes) );
      goto error_closefile;

   }

   // write data to file

   writeRes = write(fd, buf, bufLen);

   if(unlikely(writeRes != (ssize_t)bufLen) )
   { // error
      LogContext(logContext).logErr("Unable to write dentry metadata update: " +
         idStorePath + ". " + "SysErr: " + System::getErrString() );

      goto error_closefile;
   }

   // truncate in case the update lead to a smaller file size
   truncRes = ftruncate(fd, bufLen);
   if(truncRes == -1)
   { // ignore trunc errors
      LogContext(logContext).log(Log_WARNING, "Unable to truncate metadata file (strange, but "
         "proceeding anyways): " + idStorePath + ". " + "SysErr: " + System::getErrString() );
   }

   close(fd);

   LOG_DEBUG(logContext, 4, "Dentry metadata update stored: " + idStorePath);

   return true;


   // error compensation
error_closefile:
   close(fd);

   return false;
}

/**
 * Store an update DirEntry.
 *
 * Note: Never write to a dentry using the entryNamePath. We might simply write to the wrong
 *       path. E.g. after a rename() and overwriting an opened file. Closing the overwritten
 *       file will result in an inode update. If then some data structures are not correct
 *       yet, writing to dentry-by-name instead of dentry-by-id might update the wrong file,
 *       instead of simply returning an error message.
 *       However, an exception is fsck, which needs to modify the dentry-by-name directly to update
 *       a dentry-owner.
 */
bool DirEntry::storeUpdatedDirEntry(const std::string& dirEntryPath)
{
   char buf[DIRENTRY_SERBUF_SIZE];
   Serializer ser(buf, sizeof(buf));

   serializeDentry(ser);

   std::string idStorePath = dirEntryPath + "/" + name;

   bool result = storeUpdatedDirEntryBuf(idStorePath, buf, ser.size());

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(idStorePath, MetaSyncFileType::Dentry);

   return result;
}

/**
 * Store the inlined inode from a dir-entry.
 * */
FhgfsOpsErr DirEntry::storeUpdatedInode(const std::string& dirEntryPath)
{
   if (!this->getIsInodeInlined() )
      return FhgfsOpsErr_INODENOTINLINED;

   char buf[DIRENTRY_SERBUF_SIZE];
   Serializer ser(buf, sizeof(buf));

   serializeDentry(ser);

   std::string idStorePath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) + getEntryID();

   bool storeRes = storeUpdatedDirEntryBuf(idStorePath, buf, ser.size());
   if (!storeRes)
      return FhgfsOpsErr_SAVEERROR;

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(idStorePath, MetaSyncFileType::Inode);

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr DirEntry::removeDirEntryFile(const std::string& filePath)
{
   int unlinkRes = unlink(filePath.c_str() );
   if (unlinkRes == 0)
      return FhgfsOpsErr_SUCCESS;

   if (errno == ENOENT)
      return FhgfsOpsErr_PATHNOTEXISTS;

   LOG(GENERAL, ERR, "Unable to delete dentry file", filePath, sysErr);
   return FhgfsOpsErr_INTERNAL;
}

/**
 * Remove the given filePath. This method is used for dirEntries-by-entryID and dirEntries-by-name.
 */
FhgfsOpsErr DirEntry::removeDirEntryName(const char* logContext, const std::string& filePath,
   bool isBuddyMirrored)
{
   FhgfsOpsErr retVal = removeDirEntryFile(filePath);

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addDeletion(filePath, MetaSyncFileType::Dentry);

   return retVal;
}

/**
 * Remove the dirEntrID file.
 */
FhgfsOpsErr DirEntry::removeDirEntryID(const std::string& dirEntryPath,
   const std::string& entryID, bool isBuddyMirrored)
{
   std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) +  entryID;

   FhgfsOpsErr idUnlinkRes = removeDirEntryFile(idPath);

   if (likely(idUnlinkRes == FhgfsOpsErr_SUCCESS))
      LOG_DBG(GENERAL, DEBUG, "Dir-Entry ID metadata deleted", idPath);

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addDeletion(idPath, MetaSyncFileType::Inode);

   return idUnlinkRes;
}


/**
 * Remove a dir-entry with an inlined inode. We cannot remove the inode, though and so
 * will rename the dir-entry into the inode-hash directories to create a non-inlined
 * inode (with dentry format, though).
 *
 * @param unlinkFileName  Unlink only the ID file or also the entryName. If false entryName might
 *                        be empty.
 */
FhgfsOpsErr DirEntry::removeBusyFile(const std::string& dirEntryBasePath,
   const std::string& entryID, const std::string& entryName, unsigned unlinkTypeFlags)
{
   const char* logContext = "Unlinking dirEnty with busy inlined inode";

   App* app = Program::getApp();

   std::string dentryPath = dirEntryBasePath + '/' + entryName;
   std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryBasePath ) + entryID;

   std::string inodePath  = MetaStorageTk::getMetaInodePath(
         getIsBuddyMirrored()
            ? app->getBuddyMirrorInodesPath()->str()
            : app->getInodesPath()->str(),
         entryID);

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   if (unlinkTypeFlags & DirEntry_UNLINK_FILENAME)
   {
      // Delete the dentry-by-name

      int unlinkNameRes = unlink(dentryPath.c_str() );
      if (unlinkNameRes)
      {
         if (errno != ENOENT)
         {
            LogContext(logContext).logErr("Failed to unlink the dirEntry file: " + dentryPath +
               " SysErr: " + System::getErrString() );

            retVal = FhgfsOpsErr_INTERNAL;
         }
         else
            retVal = FhgfsOpsErr_PATHNOTEXISTS;

         goto out;
      }

      if (getIsBuddyMirrored())
         if (auto* resync = BuddyResyncer::getSyncChangeset())
            resync->addDeletion(dentryPath, MetaSyncFileType::Dentry);

      retVal = FhgfsOpsErr_SUCCESS;
   }

   if (unlinkTypeFlags & DirEntry_UNLINK_ID)
   {
      // Rename the ID to the inode directory

      int renameRes = rename(idPath.c_str(), inodePath.c_str() );
      if (!renameRes)
      {
         /* Posix rename() has a very weird feature - it does nothing if fromPath and toPath
          * are a hard-link pointing to each other. And it even does not give an error message.
          * Seems they wanted to have rename(samePath, samePath).
          * Once we do support hard links, we are going to hard-link dentryPath and inodePath
          * to each other. An unlink() then should be done using the seperate dentry/inode path,
          * but if there should be race between link() and unlink(), we might leave a dentry in
          * place, which was supposed to be unlinked. So without an error message from rename()
          * if it going to do nothing, we always need to try to unlink the file now. We can only
          * hope the kernel still has a negative dentry in place, which will immediately tell
          * that the file already does not exist anymore. */
         unlink(idPath.c_str() );

         // Now link to the disposal dir
         DirInode* disposalDir = getIsBuddyMirrored()
            ? app->getBuddyMirrorDisposalDir()
            : app->getDisposalDir();
         disposalDir->linkFileInodeToDir(inodePath, entryID); // entryID will be the new fileName
         // Note: we ignore a possible error here, as don't know what to do with it.

         if (getIsBuddyMirrored())
            if (auto* resync = BuddyResyncer::getSyncChangeset())
            {
               resync->addDeletion(idPath, MetaSyncFileType::Inode);
               resync->addModification(inodePath, MetaSyncFileType::Inode);
            }

         retVal = FhgfsOpsErr_SUCCESS;
      }
      else
      {
         int errCode = errno;

         LogContext(logContext).logErr("Failed to move dirEntry:"
            " From: "   + idPath +
            " To: "     + inodePath +
            " SysErr: " + System::getErrString() );

         if (unlinkTypeFlags & DirEntry_UNLINK_FILENAME)
         {
            // take the error code (SUCCESS) from file name unlink
         }
         else
         {
            if (errCode == ENOENT)
               retVal = FhgfsOpsErr_PATHNOTEXISTS;
            else
               retVal = FhgfsOpsErr_INTERNAL;
         }
      }

   }

out:
   return retVal;
}



/**
 * Note: Wrapper/chooser for loadFromFileXAttr/Contents.
 * To be used for all Dentry (dir-entry) operations.
 */
bool DirEntry::loadFromFileName(const std::string& dirEntryPath, const std::string& entryName)
{
   std::string entryNamePath = dirEntryPath + "/" + entryName;

   return loadFromFile(entryNamePath);
}

/**
 * To be used for all operations regarding the inlined inode.
 */
bool DirEntry::loadFromID(const std::string& dirEntryPath, const std::string& entryID)
{
   std::string idStorePath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) + entryID;

   return loadFromFile(idStorePath);
}

/**
 * Note: Wrapper/chooser for loadFromFileXAttr/Contents.
 * Note: Do not call directly, but use loadFromFileName() or loadFromID()
 * Retrieve the dir-entry either from xattrs or from real file data - configuration option
 */
bool DirEntry::loadFromFile(const std::string& path)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return loadFromFileXAttr(path);

   return loadFromFileContents(path);
}

/**
 * Note: Don't call this directly, use the wrapper loadFromFileName().
 */
bool DirEntry::loadFromFileXAttr(const std::string& path)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(load from xattr file)";
   Config* cfg = Program::getApp()->getConfig();

   bool retVal = false;

   char buf[DIRENTRY_SERBUF_SIZE];

   ssize_t getRes = getxattr(path.c_str(), META_XATTR_NAME, buf, DIRENTRY_SERBUF_SIZE);
   if(getRes > 0)
   { // we got something => deserialize it
      Deserializer des(buf, getRes);
      deserializeDentry(des);
      if(unlikely(!des.good()))
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize dir-entry file: " + path);
         goto error_exit;
      }

      retVal = true;
   }
   else
   if( (getRes == -1) && (errno == ENOENT) )
   { // file not exists
      LOG_DEBUG_CONTEXT(LogContext(logContext), Log_DEBUG, "dir-entry file not exists: " +
         path + ". " + "SysErr: " + System::getErrString() );
   }
   else
   if( ( (getRes == 0) || ( (getRes == -1) && (errno == ENODATA) ) ) &&
       (cfg->getStoreSelfHealEmptyFiles() ) )
   { // empty link file probably due to server crash => self-heal through removal
      if (likely(this->name != META_DIRENTRYID_SUB_STR) )
      {
         LogContext(logContext).logErr("Found an empty dir-entry file. "
            "(Self-healing through file removal): " + path);

         int unlinkRes = unlink(path.c_str() );
         if(unlinkRes == -1)
         {
            LogContext(logContext).logErr("File removal for self-healing failed: " + path + ". "
               "SysErr: " + System::getErrString() );
         }
      }
   }
   else
   { // unhandled error
      LogContext(logContext).logErr("Unable to open/read dir-entry file: " + path + ". " +
         "SysErr: " + System::getErrString() );
   }


error_exit:

   return retVal;
}

/**
 * Note: Don't call this directly, use the wrapper loadFromFile().
 */
bool DirEntry::loadFromFileContents(const std::string& path)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(load from file)";
   Config* cfg = Program::getApp()->getConfig();

   bool retVal = false;
   char buf[DIRENTRY_SERBUF_SIZE];
   int readRes;

   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(path.c_str(), openFlags);
   if(fd == -1)
   { // open failed
      if(likely(errno == ENOENT) )
      { // file not exists
         LOG_DEBUG_CONTEXT(LogContext(logContext), Log_DEBUG, "Unable to open dentry file: " +
            path + ". " + "SysErr: " + System::getErrString() );
      }
      else
      {
         LogContext(logContext).logErr("Unable to open link file: " + path + ". " +
            "SysErr: " + System::getErrString() );
      }

      goto error_donothing;
   }

   readRes = read(fd, buf, DIRENTRY_SERBUF_SIZE);
   if(likely(readRes > 0) )
   { // we got something => deserialize it
      Deserializer des(buf, readRes);
      deserializeDentry(des);
      if(unlikely(!des.good()))
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize dentry file: " + path);
         goto error_close;
      }

      retVal = true;
   }
   else
   if( (readRes == 0) && cfg->getStoreSelfHealEmptyFiles() )
   { // empty link file probably due to server crash => self-heal through removal
      LogContext(logContext).logErr("Found an empty link file. "
         "(Self-healing through file removal): " + path);

      int unlinkRes = unlink(path.c_str() );
      if(unlinkRes == -1)
      {
         LogContext(logContext).logErr("File removal for self-healing failed: " + path + ". "
            "SysErr: " + System::getErrString() );
      }
   }
   else
   { // read error
      LogContext(logContext).logErr("Unable to read denty file: " + path + ". " +
         "SysErr: " + System::getErrString() );
   }


error_close:
   close(fd);

error_donothing:

   return retVal;
}

DirEntryType DirEntry::loadEntryTypeFromFile(const std::string& path, const std::string& entryName)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return loadEntryTypeFromFileXAttr(path, entryName);

   return loadEntryTypeFromFileContents(path, entryName);

}

/**
 * Note: We don't do any serious error checking here, we just want to find out the type or will
 * return DirEntryType_INVALID otherwise (eg if file not found)
 */
DirEntryType DirEntry::loadEntryTypeFromFileXAttr(const std::string& path,
   const std::string& entryName)
{
   DirEntryType retVal = DirEntryType_INVALID;

   std::string storePath(path + "/" + entryName);

   char buf[DIRENTRY_SERBUF_SIZE];

   int getRes = getxattr(storePath.c_str(), META_XATTR_NAME, buf, DIRENTRY_SERBUF_SIZE);
   if(getRes <= 0)
   { // getting failed
      goto out;
   }

   retVal = (DirEntryType)buf[DIRENTRY_TYPE_BUF_POS];

out:

   return retVal;
}

/**
 * Note: We don't do any serious error checking here, we just want to find out the type or will
 * return DirEntryType_INVALID otherwise (eg if file not found)
 */
DirEntryType DirEntry::loadEntryTypeFromFileContents(const std::string& path,
   const std::string& entryName)
{
   DirEntryType retVal = DirEntryType_INVALID;

   std::string storePath(path + "/" + entryName);

   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(storePath.c_str(), openFlags);
   if(fd == -1)
   { // open failed
      return DirEntryType_INVALID;
   }

   char buf;
   int readRes = pread(fd, &buf, 1, DIRENTRY_TYPE_BUF_POS);
   if(likely(readRes > 0) )
   { // we got something
      retVal = (DirEntryType)buf;
   }

   close(fd);

   return retVal;
}


DirEntry* DirEntry::createFromFile(const std::string& path, const std::string& entryName)
{
   std::string filepath = path + "/" + entryName;

   DirEntry* newDir = new DirEntry(entryName);

   bool loadRes = newDir->loadFromFileName(path, entryName);
   if(!loadRes)
   {
      delete(newDir);
      return NULL;
   }

   return newDir;
}

/**
 * Return the inlined inode from a dir-entry.
 */
FileInode* DirEntry::createInodeByID(const std::string& dirEntryPath, EntryInfo* entryInfo)
{
   bool loadRes = loadFromID(dirEntryPath, entryInfo->getEntryID() );
   if (!loadRes)
      return NULL;

   if (!this->getIsInodeInlined() )
      return NULL;

   unsigned dentryFeatureFlags  = getDentryFeatureFlags();

   unsigned inodeFeatureFlags = this->inodeData.getInodeFeatureFlags();


   if (inodeFeatureFlags & FILEINODE_FEATURE_HAS_ORIG_PARENTID)
   {
      // i.e. file was renamed between directories, disk data already have origParentEntryID
   }
   else
      this->inodeData.setDynamicOrigParentEntryID(entryInfo->getParentEntryID() );

   /* NOTE: origParentUID also might not be on disk, but the deserializer then set it from
    *       statData->uid */

   FileInode* inode = new (std::nothrow) FileInode(this->getID(), &this->inodeData,
      this->getEntryType(), dentryFeatureFlags);
   if (unlikely(!inode) )
   {
      LogContext(__func__).logErr("Out of memory, failed to allocate inode.");

      return NULL; // out of memory
   }

   return inode;
}


/**
 * Note: Must be called before any of the disk modifying methods
 * (otherwise they will fail)
 *
 * @param path does not include the filename
 */
FhgfsOpsErr DirEntry::storeInitialDirEntry(const std::string& dirEntryPath)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(store initial dirEntry)";

   LOG_DEBUG(logContext, 4, "Storing initial dentry metadata for ID: '" + getEntryID() + "'");

   std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) +  getEntryID();

   // first create the dirEntry-by-ID
   FhgfsOpsErr entryIdRes = this->storeInitialDirEntryID(logContext, idPath);

   if (entryIdRes != FhgfsOpsErr_SUCCESS)
      return entryIdRes;

   bool nonInlined = DirEntryType_ISDIR(getEntryType()) || !this->getIsInodeInlined();

   // eventually the dirEntry-by-name
   std::string namePath = dirEntryPath + '/' + this->name;
   FhgfsOpsErr result = this->storeInitialDirEntryName(logContext, idPath, namePath, nonInlined);

   if (result == FhgfsOpsErr_SUCCESS && getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         if (!nonInlined)
            resync->addModification(idPath, MetaSyncFileType::Inode);

         resync->addModification(namePath, MetaSyncFileType::Dentry);
      }

   return result;
}
