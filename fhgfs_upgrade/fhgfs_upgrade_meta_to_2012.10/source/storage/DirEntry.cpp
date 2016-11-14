#include <common/toolkit/serialization/Serialization.h>
#include <program/Program.h>
#include <upgrade/Upgrade.h>
#include "MetadataEx.h"
#include "DirEntry.h"

#include <attr/xattr.h> // (cannot be sys/xattr.h, because that doesn't provide ENOATTR)

DirEntry::DirEntry(DirEntryType entryType, std::string name, std::string entryID,
   uint16_t ownerNodeID) : name(name)
{
   this->entryType      = entryType;
   this->entryID        = entryID;
   this->ownerNodeID    = ownerNodeID;

   this->dentryFeatureFlags   = 0;

   this->updatedEntryID = entryID; // quick and diry, code path used for files only and here
                                   // we solved that differently
}

/*
 * Store the dirEntryID file. This is a normal dirEntry (with inlined inode),
 * but the file name is the entryID.
 *
 * @param idPath - path to the idFile, including the file name
 * @param path does not include the filename
 */
FhgfsOpsErr DirEntry::storeInitialDirEntryID(const char* logContext, const std::string& idPath)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   char* buf;
   unsigned bufLen;
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   // create file

   /* note: if we ever think about switching to a rename-based version here, we must keep very
     long user file names in mind, which might lead to problems if we add an extension to the
     temporary file name. */

   int openFlags = O_CREAT|O_WRONLY;

   int fd = open(idPath.c_str(), openFlags, 0644);
   if(fd == -1)
   { 
      // running the upgrade tool multiple times might cause EEXIST
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
      return retVal;
   }

   // serialize (to new buf)

   buf = (char*)malloc(DIRENTRY_SERBUF_SIZE);
   if (unlikely(buf == NULL) )
   {
      LogContext(logContext).logErr("malloc() failed, out of memory");
      retVal = FhgfsOpsErr_INTERNAL;
      goto error_closefile;
   }
   bufLen = serializeDentry(buf);

   // write buf to file

   if(useXAttrs)
   { // extended attribute
      int setRes = fsetxattr(fd, META_XATTR_LINK_NAME, buf, bufLen, 0);
      free(buf);

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
      ssize_t writeRes = write(fd, buf, bufLen);
      free(buf);

      if(unlikely(writeRes != (ssize_t)bufLen) )
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
   const std::string& namePath, bool isDir)
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

   if (isDir)
   {
      // unlink the dentry-by-id file - we don't need it for dirs (or non-inlined inodes in general)
      int unlinkRes = unlink(idPath.c_str() );
      if (unlikely(unlinkRes) )
      {
         LogContext(logContext).logErr("Failed to unlink the (dir) dentry-by-id file "+ idPath +
            " SysErr: " + System::getErrString() );
      }
   }

   /* TODO: mkdir() might be a bit slow due to this 3-way operation (dentry-by-id, link-to-name,
    *       remove dentry-by-id. If it is too slow we might need to switch to a simple
    *       create-dentry-by-name, but which wouldn't set xattrs atomically from clients point of
    *       view.  */

   LOG_DEBUG(logContext, 4, "Initial dirEntry stored: " + namePath);

   return retVal;
}

/**
 * Note: Wrapper/chooser for storeUpdatedDirEntryBufAsXAttr/Contents.
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirEntry::storeUpdatedDirEntryBuf(std::string idStorePath, char* buf, unsigned bufLen)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return storeUpdatedDirEntryBufAsXAttr(idStorePath, buf, bufLen);

   return storeUpdatedDirEntryBufAsContents(idStorePath, buf, bufLen);
}

/**
 * Note: Don't call this directly, use the wrapper storeUpdatedDirEntryBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirEntry::storeUpdatedDirEntryBufAsXAttr(std::string idStorePath, char* buf, unsigned bufLen)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(store updated xattr metadata)";

   // write data to file

   int setRes = setxattr(idStorePath.c_str(), META_XATTR_LINK_NAME, buf, bufLen, 0);

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
bool DirEntry::storeUpdatedDirEntryBufAsContents(std::string idStorePath, char* buf,
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
            + System::getErrString(errno));
         goto error_closefile;
      }

      if (statBuf.st_size < bufLen)
      {
         LogContext(logContext).log(Log_WARNING, "File space allocation ("
            + StringTk::intToStr(bufLen)  + ") for metadata update failed: " + idStorePath + ". " +
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
         + StringTk::intToStr(bufLen)  + ") for metadata update failed: " + idStorePath + ". " +
         "SysErr: " + System::getErrString(fallocRes));
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
 *
 *       */
bool DirEntry::storeUpdatedDirEntry(std::string dirEntryPath)
{
   char* buf = (char*)malloc(DIRENTRY_SERBUF_SIZE);

   unsigned bufLen = serializeDentry(buf);

   std::string idStorePath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) + entryID;

   bool saveRes = storeUpdatedDirEntryBuf(idStorePath, buf, bufLen);

   free(buf);

   return saveRes;
}

/**
 * Store the inlined inode from a dir-entry.
 *
 * NOTE: Actually we do not set the complete stripe pattern here, but only the stripe target IDs
 */
FhgfsOpsErr DirEntry::storeUpdatedInode(std::string dirEntryPath, std::string entryID,
   StatData* newStatData, StripePattern* updatedStripePattern)
{
   const char* logContext = "DirEntry (storeUpdatedInode)";

   /* Note: As we are called from FileInode most data of this DirEntry are unknown and we need to
    *       load it from disk. */
   bool loadRes = loadFromID(dirEntryPath, entryID);
   if (!loadRes)
      return FhgfsOpsErr_INTERNAL;

   if (!this->isInodeInlined() )
      return FhgfsOpsErr_INODENOTINLINED;

   DentryInodeMeta* inodeMeta = this->getDentryInodeMeta();

   StatData* inlinedStatData = inodeMeta->getInodeStatData();

   inlinedStatData->set(newStatData);

   if (unlikely(updatedStripePattern))
   {
      if (! this->getDentryInodeMeta()->getPattern()->updateStripeTargetIDs(updatedStripePattern))
         LogContext(logContext).log(Log_WARNING, "Could not set new stripe target IDs.");
   }

   bool storeRes = storeUpdatedDirEntry(dirEntryPath);
   if (!storeRes)
      return FhgfsOpsErr_SAVEERROR;

   return FhgfsOpsErr_SUCCESS;
}


/**
 * Remove the given filePath. This method is used for dirEntries-by-entryID and dirEntries-by-name.
 */
FhgfsOpsErr DirEntry::removeDirEntryName(const char* logContext, std::string filePath)
{

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   // delete metadata file
   int unlinkRes = unlink(filePath.c_str() );
   if(unlinkRes == -1)
   { // error
      if(errno == ENOENT)
         retVal = FhgfsOpsErr_PATHNOTEXISTS;
      else
      {
         LogContext(logContext).logErr("Unable to delete dentry file: " + filePath +
            ". " + "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;
      }

      return retVal;
   }

   return retVal;
}


/**
 * Remove a dir-entry with an inlined inode. We cannot remove the inode, though and so
 * will rename the dir-entry into the inode-hash directories to create a non-inlined
 * inode (with dentry format, though).
 *
 * @param unlinkFileName  Unlink only the ID file or also the entryName. If false entryName might
 *                        be empty.
 */
FhgfsOpsErr DirEntry::removeBusyFile(std::string dirEntryBasePath, std::string entryID,
   std::string entryName, bool unlinkFileName)
{
   const char* logContext = "Unlinking dirEnty with busy inlined inode";

   App* app = Program::getApp();

   std::string dentryPath = dirEntryBasePath + '/' + entryName;
   std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryBasePath ) + entryID;

   std::string inodePath  = MetaStorageTk::getMetaInodePath(
      app->getEntriesPath()->getPathAsStrConst(), entryID);

   FhgfsOpsErr retVal;

   /* First step: Update inode properties:
    *    - unset DISKMETADATA_FEATURE_INODE_INLINE.
    *    - decrease numHardLinks
    * Required later on, if the inode is back-linked into the disposal directory. */

   unsigned oldFeatureFlags = this->getFeatureFlags();
   this->setFeatureFlags(oldFeatureFlags & ~(DISKMETADATA_FEATURE_INODE_INLINE) );

   DentryInodeMeta* inodeData = this->getDentryInodeMeta();
   StatData* statData = inodeData->getInodeStatData();

   unsigned numHardLinks = statData->getNumHardlinks();
   if (likely (numHardLinks > 0))
   {
      numHardLinks--;
      statData->setNumHardLinks(numHardLinks);
   }

   bool storeRes = storeUpdatedDirEntry(dirEntryBasePath);
   if (unlikely(!storeRes) )
      return FhgfsOpsErr_INTERNAL;

   // reminder: if unlinkFileName == false -> entryName might not be set at all
   const char* origDentryInodePath =  unlinkFileName ? dentryPath.c_str() : idPath.c_str();

   // Second step: Rename the dentry to the inode directory

   int renameRes = rename(origDentryInodePath, inodePath.c_str() );
   if (likely(!renameRes) ) // something told the client this entry exists, so likely here
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
      unlink(origDentryInodePath); // ignore the error here, we take the rename() error code

      // 3rd step: now delete the id file

      if (unlinkFileName) // if not defined, we used the idPath above already
      {
         int unlinkIDRes = unlink(idPath.c_str() );
         if (unlikely (unlinkIDRes && errno != ENOENT) )
         {
            LogContext(logContext).logErr("Failed to unlink the dirEntryID file: " + idPath +
               " SysErr: " + System::getErrString() );

            // we ignore this error here, but rmdir() to baseDir will fail
         }
      }

      // 4th step: link to the disposal dir

      DirInode* disposalDir = app->getDisposalDir();
      disposalDir->linkFileInodeToDir(inodePath, entryID); // entryID will be the new fileName
      // Note: we ignore a possible error here, as don't know what to do with it.

      retVal = FhgfsOpsErr_SUCCESS;
   }
   else
   if (errno != ENOENT)
   {
      LogContext(logContext).logErr(std::string("Failed to move dirEntry: From: ") +
         origDentryInodePath  + " To: " + inodePath + " SysErr: " + System::getErrString() );
      retVal = FhgfsOpsErr_INTERNAL;
   }
   else
      retVal = FhgfsOpsErr_PATHNOTEXISTS;

   return retVal;

}



/**
 * Note: Wrapper/chooser for loadFromFileXAttr/Contents.
 * To be used for all Dentry (dir-entry) operations.
 */
bool DirEntry::loadFromFileName(std::string dirEntryPath, std::string entryName)
{
   std::string entryNamePath = dirEntryPath + "/" + entryName;

   return loadFromFile(entryNamePath);
}

/**
 * To be used for all operations regarding the inlined inode.
 */
bool DirEntry::loadFromID(std::string dirEntryPath, std::string entryID)
{
   std::string idStorePath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) + entryID;

   return loadFromFile(idStorePath);
}

/**
 * Note: Wrapper/chooser for loadFromFileXAttr/Contents.
 * Note: Do not call directly, but use loadFromFileName() or loadFromID()
 * Retrieve the dir-entry either from xattrs or from real file data - configuration option
 */
bool DirEntry::loadFromFile(std::string path)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return loadFromFileXAttr(path);

   return loadFromFileContents(path);
}

/**
 * Note: Don't call this directly, use the wrapper loadFromFileName().
 */
bool DirEntry::loadFromFileXAttr(std::string path)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(load from xattr file)";
   Config* cfg = Program::getApp()->getConfig();

   bool retVal = false;


   char* buf = (char*)malloc(DIRENTRY_SERBUF_SIZE);

   ssize_t getRes = getxattr(path.c_str(), META_XATTR_DENTRY_NAME_OLD, buf, DIRENTRY_SERBUF_SIZE);
   if(getRes > 0)
   { // we got something => deserialize it
      bool deserRes = deserializeDentry(buf);
      if(unlikely(!deserRes) )
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize dir-entry file: " + path);
         goto error_freebuf;
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
   if( ( (getRes == 0) || ( (getRes == -1) && (errno == ENOATTR) ) ) &&
       (cfg->getStoreSelfHealEmptyFiles() ) )
   { // empty link file probably due to server crash => self-heal through removal
      LogContext(logContext).logErr("Found an empty dir-entry file. "
         "(Self-healing through file removal): " + path);

      int unlinkRes = unlink(path.c_str() );
      if(unlinkRes == -1)
      {
         LogContext(logContext).logErr("File removal for self-healing failed: " + path + ". "
            "SysErr: " + System::getErrString() );
      }
   }
   else
   { // unhandled error
      LogContext(logContext).logErr("Unable to open/read dir-entry file: " + path + ". " +
         "SysErr: " + System::getErrString() );
   }


error_freebuf:
   free(buf);

   return retVal;
}

/**
 * Note: Don't call this directly, use the wrapper loadFromFile().
 */
bool DirEntry::loadFromFileContents(std::string path)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(load from file)";
   Config* cfg = Program::getApp()->getConfig();

   bool retVal = false;
   char* buf;
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

   buf = (char*)malloc(DIRENTRY_SERBUF_SIZE);

   readRes = read(fd, buf, DIRENTRY_SERBUF_SIZE);
   if(likely(readRes > 0) )
   { // we got something => deserialize it
      bool deserRes = deserializeDentry(buf);
      if(unlikely(!deserRes) )
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize dentry file: " + path);
         goto error_freebuf;
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


error_freebuf:
   free(buf);
   close(fd);

error_donothing:

   return retVal;
}

DirEntryType DirEntry::loadEntryTypeFromFile(std::string path, std::string entryName)
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

   char* buf = (char*)malloc(DIRENTRY_SERBUF_SIZE);

   int getRes = getxattr(storePath.c_str(), META_XATTR_LINK_NAME, buf, DIRENTRY_SERBUF_SIZE);
   if(getRes <= 0)
   { // getting failed
      free(buf);
      return DirEntryType_INVALID;
   }

   retVal = (DirEntryType)buf[DIRENTRY_TYPE_BUF_POS];

   free(buf);

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


DirEntry* DirEntry::createFromFile(std::string path, std::string entryName)
{
   std::string filepath = path + "/" + entryName;

   DirEntry* newDir = new DirEntry(entryName);

   bool loadRes = newDir->loadFromFileName(path, entryName);
   if(!loadRes)
   {
      delete(newDir);
      return NULL;
   }

   if (!Upgrade::updateEntryID(newDir->entryID, newDir->updatedEntryID) )
   {
      delete newDir;
      return NULL;
   }

   return newDir;
}

/**
 * Return the inlined inode from a dir-entry.
 */
FileInode* DirEntry::createInodeByID(std::string dirEntryPath, std::string entryID)
{
   bool loadRes = loadFromID(dirEntryPath, entryID);
   if (!loadRes)
      return NULL;

   if (!this->isInodeInlined() )
      return NULL;

   StatData* statData = this->inodeData.getInodeStatData();
   StripePattern* stripePattern = this->inodeData.getPattern();
   unsigned chunkHash = this->inodeData.getChunkHash();
   unsigned dentryFeatureFlags = getDentryFeatureFlags();

   FileInode* inode = new (std::nothrow) FileInode(this->getID(), statData, *stripePattern,
      this->getEntryType(), dentryFeatureFlags, chunkHash);

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
FhgfsOpsErr DirEntry::storeInitialDirEntry(std::string dirEntryPath)
{
   const char* logContext = DIRENTRY_LOG_CONTEXT "(store initial dirEntry)";

   LOG_DEBUG(logContext, 4, "Storing initial dentry metadata for ID: '" + entryID + "'");

   std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) +  this->updatedEntryID;

   // first create the dirEntry-by-ID
   FhgfsOpsErr entryIdRes = this->storeInitialDirEntryID(logContext, idPath);

   if (entryIdRes != FhgfsOpsErr_SUCCESS)
      return entryIdRes;

   bool isDir = DirEntryType_ISDIR(this->entryType);

   // eventually the dirEntry-by-name
   std::string namePath = dirEntryPath + '/' + this->name;
   return this->storeInitialDirEntryName(logContext, idPath, namePath, isDir);
}

/*
 * This method just recreates the dentry-by-ID file
 */
FhgfsOpsErr recreateDirEntryID();
