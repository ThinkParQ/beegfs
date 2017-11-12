#include <deeper/deeper_cache.h>
#include <libgen.h>
#include <zlib.h>
#include "cachepathtk.h"
#include "threadvartk.h"
#include "filesystemtk.h"



/**
 * Copy a file, set the mode and owner of the source file to the destination file.
 *
 * @param sourcePath The path to the source file.
 * @param destPath The path to the destination file.
 * @param statSource The stat of the source file, if this pointer is NULL this function stats the
 *        source file.
 * @param deleteSource True if the file should be deleted after the file was copied.
 * @param doCRC True if a CRC checksum should be calculated.
 * @param work The work which executes this function. This is required to support stop function to
 *        every work.
 * @param crcOutValue Out value for the checksum if a checksum should be calculated.
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::copyFile(const char* sourcePath, const char* destPath,
   const struct stat* statSource, bool deleteSource, bool doCRC, CacheWork* work,
   unsigned long* crcOutValue)
{
   Logger* log = Logger::getLogger();

   int retVal = DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + std::string(sourcePath) +
      "; dest path: " + destPath);
#endif

   int openFlagSource = O_RDONLY;
   int openFlagDest = O_WRONLY | O_CREAT | O_TRUNC;

   bool error = false;
   int saveErrno = 0;
   int dest;

   int readSize;
   int writeSize;

   Byte* buf = (Byte*)malloc(BUFFER_SIZE);
   if(!buf)
   {
      log->logErr(__FUNCTION__,
         "Could not allocate memory to copy the file: " + std::string(sourcePath) +
         "; errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   int source = open(sourcePath, openFlagSource, 0);
   if(source == DEEPER_RETVAL_ERROR)
   {
      log->logErr(__FUNCTION__, "Could not open source file: " + std::string(sourcePath) +
         "; errno: " + System::getErrString(errno) );

      retVal = DEEPER_RETVAL_ERROR;
      goto free_buf;
   }

   struct stat newStatSource;

   if(!statSource || !S_ISREG(statSource->st_mode) )
   {
      retVal = fstat(source, &newStatSource);
      if(retVal == DEEPER_RETVAL_ERROR)
      {
         saveErrno = errno;
         error = true;

         log->logErr(__FUNCTION__, "Could not stat source file: " + std::string(sourcePath) +
            "; Errno: " + System::getErrString(saveErrno) );

         goto close_source;
      }

      // open with the stat infomation from this message
      dest = open(destPath, openFlagDest, newStatSource.st_mode);
   }
   else // open with mode from external stat information
      dest = open(destPath, openFlagDest, statSource->st_mode);

   if(dest == DEEPER_RETVAL_ERROR)
   {
      saveErrno = errno;
      error = true;

      log->logErr(__FUNCTION__, "Could not open destination file: " + std::string(destPath) +
         "; Errno: " + System::getErrString(saveErrno) );

      goto close_source;
   }

   if(doCRC && (crcOutValue != NULL) )
      *crcOutValue = crc32(0L, Z_NULL, 0); // init checksum

   while ( (readSize = read(source, buf, BUFFER_SIZE) ) > 0)
   {
      if(work && work->shouldStop() )
      {
         if(doCRC) // reset CRC value
            *crcOutValue = 0;

         goto close_dest;
      }

      if(doCRC)
         *crcOutValue = crc32(*crcOutValue, buf, readSize);

      writeSize = write(dest, buf, readSize);

      if(unlikely(writeSize != readSize) )
      {
         saveErrno = errno;
         error = true;

         log->logErr(__FUNCTION__, "Could not copy file " + std::string(sourcePath) + " to " +
            std::string(destPath) + ". errno: " + System::getErrString(saveErrno) );

         goto close_dest;
      }
   }


   if(!statSource)
      retVal = fchown(dest, newStatSource.st_uid, newStatSource.st_gid);
   else
      retVal = fchown(dest, statSource->st_uid, statSource->st_gid);

   if( (retVal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
   {
      saveErrno = errno;
      error = true;

      log->logErr(__FUNCTION__, "Could not set owner of file: " + std::string(destPath) +
         "; errno: " + System::getErrString(saveErrno) );
   }


close_dest:
   retVal = close(dest);
   if(!error && (retVal == DEEPER_RETVAL_ERROR) )
   {
      saveErrno = errno;
      error = true;
   }

close_source:
   retVal = close(source);
   if(!error && (retVal == DEEPER_RETVAL_ERROR) )
   {
      saveErrno = errno;
      error = true;
   }

   if(error)
   {
      errno = saveErrno;
      retVal = DEEPER_RETVAL_ERROR;
   }
   else if(deleteSource)
   {
      if(unlink(sourcePath) )
         log->logErr(__FUNCTION__, "Could not delete file after copy. path: " +
            std::string(sourcePath) + "; errno: " + System::getErrString(errno) );
   }

free_buf:
   SAFE_FREE(buf);

   return retVal;
}

/**
 * copy a range of file, set the mode and owner of the source file to the destination file
 *
 * @param sourcePath the path to the source file
 * @param destPath the path to the destination file
 * @param statSource the stat of the source file, if this pointer is NULL this function stats the
 *        source file
 * @param offset the start offset to copy the file, required if copyRange is true, it returns
 *        the new offset for the next read, ...
 * @param numBytes the number of bytes to copy starting by the offset, required if copyRange is
 *        true
 * @param work The work which executes this function. This is required to support stop function to
 *        the work.
 * @param doChown True if chown must be done. If this is done by a split work the chown is not
 *        necessary.
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::copyFileRange(const char* sourcePath, const char* destPath,
   const struct stat* statSource, off_t* offset, size_t numBytes, CacheWork* work, bool doChown)
{
   Logger* log = Logger::getLogger();

   int retVal = DEEPER_RETVAL_ERROR;
   off_t retOffset = 0;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + std::string(sourcePath) +
      "; dest path: " + destPath);
#endif

   int openFlagsSource = O_RDONLY;
   int openFlagsDest = O_WRONLY | O_CREAT;

   bool error = false;
   int saveErrno = 0;
   int dest;

   int readSize;
   int writeSize;
   size_t nextReadSize;
   size_t dataToRead = numBytes;

   char* buf = (char*)malloc(BUFFER_SIZE);
   if(!buf)
   {
      log->logErr(__FUNCTION__, "Could not allocate memory to copy the file: " +
         std::string(sourcePath) + "; errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   int source = open(sourcePath, openFlagsSource, 0);
   if(source == DEEPER_RETVAL_ERROR)
   {
      log->logErr(__FUNCTION__, "Could not open source file: " + std::string(sourcePath) +
         "; errno: " + System::getErrString(errno) );

      retVal = DEEPER_RETVAL_ERROR;
      goto free_buf;
   }

   struct stat newStatSource;

   if(!statSource || !S_ISREG(statSource->st_mode) )
   {
     retVal = fstat(source, &newStatSource);
     if(retVal == DEEPER_RETVAL_ERROR)
     {
        saveErrno = errno;
        error = true;

        log->logErr(__FUNCTION__, "Could not stat source file: " + std::string(sourcePath) +
           "; errno: " + System::getErrString(saveErrno) );

        goto close_source;
     }

     // open with the stat infomation from this message
     dest = open(destPath, openFlagsDest, newStatSource.st_mode);
   }
   else // open with mode from external stat information
     dest = open(destPath, openFlagsDest, statSource->st_mode);

   if(dest == DEEPER_RETVAL_ERROR)
   {
     saveErrno = errno;
     error = true;

     log->logErr(__FUNCTION__, "Could not open destination file: " + std::string(destPath) +
        "; errno: " + System::getErrString(saveErrno) );

     goto close_source;
   }

   retOffset = lseek(source, *offset, SEEK_SET);
   if(retOffset != *offset)
   {
      saveErrno = errno;
      error = true;

      log->logErr(__FUNCTION__, "Could not seek in source file: " + std::string(sourcePath) +
         "; errno: " + System::getErrString(saveErrno) );

      goto close_dest;
   }

   retOffset = lseek(dest, *offset, SEEK_SET);
   if(retOffset != *offset)
   {
      saveErrno = errno;
      error = true;

      log->logErr(__FUNCTION__, "Could not seek in destination file: " + std::string(destPath) +
         "; errno: " + System::getErrString(saveErrno) );

      goto close_dest;
   }

   nextReadSize = std::min(dataToRead, BUFFER_SIZE);
   while(nextReadSize > 0 )
   {
      if(work && work->shouldStop() )
         goto close_dest;

      readSize = read(source, buf, nextReadSize);

      if(readSize == 0)
      { // could happen if a file was truncated, but it is not an error, so do the chown afterwards
         break;
      }
      else if(readSize == -1)
      {
         saveErrno = errno;
         error = true;

         log->logErr(__FUNCTION__, "Could not copy file: " + std::string(sourcePath) + " to " +
            std::string(destPath) + ". errno: " + System::getErrString(saveErrno) );

         goto close_dest;
      }

      writeSize = write(dest, buf, readSize);

      if(unlikely(writeSize != readSize) )
      {
         saveErrno = errno;
         error = true;

         log->logErr(__FUNCTION__, "Could not read data from file: " + std::string(sourcePath) +
            "; errno: " + System::getErrString(saveErrno) );

         goto close_dest;
      }

      dataToRead -= readSize;
      nextReadSize = std::min(dataToRead, BUFFER_SIZE);
   }

   if(doChown)
   {
      if(!statSource)
         retVal = fchown(dest, newStatSource.st_uid, newStatSource.st_gid);
      else
         retVal = fchown(dest, statSource->st_uid, statSource->st_gid);

      if( (retVal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
      {
         saveErrno = errno;
         error = true;

         log->logErr(__FUNCTION__, "Could not set owner of file: " + std::string(destPath) +
            "; errno: " + System::getErrString(saveErrno) );
      }
   }

close_dest:
   retVal = close(dest);
   if(!error && (retVal == DEEPER_RETVAL_ERROR) )
   {
      saveErrno = errno;
      error = true;
   }

close_source:
   retVal = close(source);
   if(!error && (retVal == DEEPER_RETVAL_ERROR) )
   {
      saveErrno = errno;
      error = true;
   }

   if(error)
   {
      errno = saveErrno;
      retVal = DEEPER_RETVAL_ERROR;
   }

free_buf:
   SAFE_FREE(buf);

   return retVal;
}


/**
 * copies a directory to an other directory using nftw()
 *
 * @param source the path to the source directory
 * @param dest the path to the destination directory
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache
 *        BeeGFS, false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the directory should be deleted after the directory was copied,
 *        but only files are deleted, directories must be deleted in a second nftw run, if
 *        followSymlink is set nothing will be deleted, the second nftw run must also delete the
 *        files
 * @param followSymlink true if the destination file or directory should be copied and do not
 *        create symbolic links when a symbolic link was found
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::copyDir(const char* source, const char* dest, bool copyToCache, bool deleteSource,
   bool followSymlink)
{
   Logger* log = Logger::getLogger();
   CacheConfig* cfg = dynamic_cast<CacheConfig*>(PThread::getCurrentThreadApp()->getCommonConfig());

   int retVal = DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + std::string(source) +
      "; dest path: " + dest);
#endif


   /**
    * used nftw flags copy directories
    * FTW_ACTIONRETVAL  handles the return value from the fn(), fn() must return special values
    * FTW_PHYS          do not follow symbolic links
    * FTW_MOUNT         stay within the same file system
    */
   const int nftwFlagsCopy = FTW_ACTIONRETVAL | FTW_PHYS | FTW_MOUNT;

   const std::string& mountOfSource(copyToCache ? cfg->getSysMountPointGlobal() :
      cfg->getSysMountPointCache() );
   const std::string& mountOfDest(copyToCache ? cfg->getSysMountPointCache() :
      cfg->getSysMountPointGlobal() );

   struct stat statSource;
   int funcError = lstat(source, &statSource);
   if(funcError != 0)
   {
      log->logErr(__FUNCTION__, "Could not stat source file/directory " + std::string(source) +
         "; errno: " + System::getErrString(errno) );
      retVal = DEEPER_RETVAL_ERROR;
   }
   else if(S_ISREG(statSource.st_mode) )
   {
      if(!cachepathtk::createPath(dest, mountOfSource, mountOfDest, true, log) )
         return DEEPER_RETVAL_ERROR;

      retVal = copyFile(source, dest, &statSource, deleteSource, false, NULL, NULL);
   }
   else if(S_ISLNK(statSource.st_mode) )
   {
      if(!cachepathtk::createPath(dest, mountOfSource, mountOfDest, true, log) )
         return DEEPER_RETVAL_ERROR;

      if(followSymlink)
         retVal= handleFollowSymlink(source, dest, &statSource, copyToCache, deleteSource, false,
            false, NULL);
      else
         retVal = createSymlink(source, dest, &statSource, copyToCache, deleteSource);
   }
   else
   {
      if(!cachepathtk::createPath(dest, mountOfSource, mountOfDest, false, log) )
         return retVal;

      retVal = chown(dest, statSource.st_uid, statSource.st_gid);
      if( (retVal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
      {
         log->logErr(__FUNCTION__, "Could not set owner of link: " + std::string(dest) +
            "; errno: " + System::getErrString(errno) );

         return retVal;
      }

      if(copyToCache)
         retVal = nftw(source, nftwCopyFileToCache, NFTW_MAX_OPEN_FD, nftwFlagsCopy);
      else
         retVal = nftw(source, nftwCopyFileToGlobal, NFTW_MAX_OPEN_FD, nftwFlagsCopy);
   }

   return retVal;
}

/**
 * creates a symlink
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 * @param statSource the stat of the source file, if this pointer is NULL this function stats the
 *        source file
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache
 *        BeeGFS, false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the file/directory should be deleted after the file/directory was
 *        copied
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::createSymlink(const char* sourcePath, const char* destPath,
   const struct stat* statSource, bool copyToCache, bool deleteSource)
{
   Logger* log = Logger::getLogger();
   CacheConfig* cfg = dynamic_cast<CacheConfig*>(PThread::getCurrentThreadApp()->getCommonConfig());

   int retVal = DEEPER_RETVAL_SUCCESS;
   std::string linkDest;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + std::string(sourcePath) +
      "; dest path: " + destPath );
#endif

   if(!cachepathtk::readLink(sourcePath, statSource, log, linkDest) )
      return DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Link destination: " + linkDest);
#endif

   // only convert absolute paths
   if(linkDest[0] == '/')
   {
      bool retValConvert = true;
      if(copyToCache)
      { // ...but only if the path points to the global FS
         if(cachepathtk::isGlobalPath(linkDest, cfg) )
         {
            retValConvert = cachepathtk::globalPathToCachePath(linkDest, cfg, log);
         }
      }
      else
      { // ...but only if the path points to the cache FS
         if(cachepathtk::isCachePath(linkDest, cfg) )
         {
            retValConvert = cachepathtk::cachePathToGlobalPath(linkDest, cfg, log );
         }
      }

      if(!retValConvert)
         return DEEPER_RETVAL_ERROR;
   }

   retVal = symlink(linkDest.c_str(), destPath );
   if(retVal == DEEPER_RETVAL_ERROR)
   {
      log->logErr("handleFlags", "Could not create symlink " + std::string(destPath) + " to " +
         linkDest + ". errno: " + System::getErrString(errno) );

      return DEEPER_RETVAL_ERROR;
   }

   retVal = lchown(destPath, statSource->st_uid, statSource->st_gid);
   if( (retVal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
   {
      log->logErr(__FUNCTION__, "Could not set owner of link: " + std::string(destPath) +
         "; errno: " + System::getErrString(errno) );

      return DEEPER_RETVAL_ERROR;
   }

   if(deleteSource)
   {
      if(unlink(sourcePath) )
         log->logErr("handleFlags", "Could not delete symlink after copy. path: " +
            std::string(sourcePath) + "; errno: " + System::getErrString(errno) );
   }

   return DEEPER_RETVAL_SUCCESS;
}

/**
 * Copies a directory to an other directory using nftw(), also do cleanup the source directory.
 *
 * @param source The path to the source directory.
 * @param dest The path to the destination directory.
 * @param copyToCache True if the data should be copied from the global BeeGFS to the cache
 *        BeeGFS, false if the data should be copied from the cache BeeGFS to the global BeeGFS.
 * @param deleteSource True if the directory should be deleted after the directory was copied.
 * @param followSymlink True if the destination file or directory should be copied and do not
 *        create symbolic links when a symbolic link was found.
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::handleSubdirectoryFlag(const char* source, const char* dest, bool copyToCache,
   bool deleteSource, bool followSymlink)
{
   Logger* log = Logger::getLogger();

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + std::string(source) +
      "; dest path: " + dest );
#endif

   /**
    * used nftw flags for delete directories
    * FTW_ACTIONRETVAL  handles the return value from the fn(), fn() must return special values
    * FTW_PHYS          do not follow symbolic links
    * FTW_MOUNT         stay within the same file system
    * FTW_DEPTH         do a post-order traversal, call fn() for the directory itself after handling
    *                   the contents of the directory and its subdirectories. By default, each
    *                   directory is handled before its contents.
    */
   const int nftwFlagsDelete = FTW_ACTIONRETVAL | FTW_PHYS | FTW_MOUNT | FTW_DEPTH;

   if(followSymlink)
      threadvartk::setThreadKeyNftwFollowSymlink(true);

   // set the discard thread key, do this only on this place for correct usage of the flag
   if(deleteSource && !followSymlink)
      threadvartk::setThreadKeyNftwFlushDiscard(true);

   // copy the directory, if the discard flag is given to copy dir depends on the followSymlink flag
   int retVal = copyDir(source, dest, copyToCache, followSymlink ? false : deleteSource,
      followSymlink);

   /* delete all sources if the DEEPER_FLUSH_DISCARD flag is set, if the DEEPER_..._FOLLOWSYMLINK
    * flag is set files and directories must be deleted else only the directories must be deleted
    * the files should be deleted during the first nftw run */
   if(deleteSource && (retVal == DEEPER_RETVAL_SUCCESS) )
   {
      retVal = nftw(source, nftwDelete, NFTW_MAX_OPEN_FD, nftwFlagsDelete);
      if(retVal) // delete the directory only when the directory is empty
      {
         retVal = rmdir(source);
         if(retVal)
            log->logErr("copyDir", "Could not delete directory after copy. path: " +
               std::string(source) + "; errno: " + System::getErrString(errno) );
      }
   }

   if(deleteSource)
      threadvartk::setThreadKeyNftwFlushDiscard(false);

   if(followSymlink)
   {
      threadvartk::setThreadKeyNftwFollowSymlink(false);
      threadvartk::resetThreadKeyNftwNumFollowedSymlinks();
   }

   return retVal;
}

/**
 * handles the followSymlink flag
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 * @param statSource the stat of the symlink
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache
 *        BeeGFS, false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the file/directory should be deleted after the file/directory was
 *        copied
 * @param ignoreDir true if directories should be ignored, should be set if the flag
 *        DEEPER_..._SUBDIRS wasn't used
 * @param doCRC true if a CRC checksum should be calculated
 * @param crcOutValue out value for the checksum if a checksum should be calculated
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::handleFollowSymlink(std::string sourcePath, std::string destPath,
   const struct stat* statSourceSymlink, bool copyToCache, bool deleteSource, bool ignoreDir,
   bool doCRC, unsigned long* crcOutValue)
{
   Logger* log = Logger::getLogger();
   CacheConfig* cfg = dynamic_cast<CacheConfig*>(PThread::getCurrentThreadApp()->getCommonConfig());

   int retVal = DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + sourcePath +
      "; dest path: " + destPath);
#endif

   // get the new source path from the symlink value
   std::string pathFromLink;
   if(!cachepathtk::readLink(sourcePath.c_str(), statSourceSymlink, log, pathFromLink) )
      return DEEPER_RETVAL_ERROR;

   // check relative paths
   if(pathFromLink[0] != '/')
   {
      char* path = strdup(sourcePath.c_str() );
      std::string parent(dirname(path) );
      free(path);

      if(!cachepathtk::makePathAbsolute(pathFromLink, parent, log) )
         return DEEPER_RETVAL_ERROR;
   }

   // check if the link doesn't point outside of the cache FS or the global FS
   if(copyToCache)
   {
      if(!cachepathtk::isGlobalPath(pathFromLink, cfg) )
      {
         log->logErr(__FUNCTION__, "Link points outside of global FS: " + pathFromLink);
         return DEEPER_RETVAL_SUCCESS; // Do not report as an error, only log it to console
      }
   }
   else
   {
      if(!cachepathtk::isCachePath(pathFromLink, cfg) )
      {
         log->logErr(__FUNCTION__, "Link points outside of cache FS: " + pathFromLink);
         return DEEPER_RETVAL_SUCCESS; // Do not report as an error, only log it to console
      }
   }

   struct stat statNewSourcePath;
   retVal = lstat(pathFromLink.c_str(), &statNewSourcePath);

   if(retVal == DEEPER_RETVAL_ERROR)
   {
      log->logErr(__FUNCTION__, "Could not stat source file: " + pathFromLink +
         "; errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   // save the number of followed symlinks
   int backupNumFollowedSymlinks;
   backupNumFollowedSymlinks = threadvartk::getThreadKeyNftwNumFollowedSymlinks();

   // backup the paths from the previous follow symlink call for the restore after this call
   bool linkPathsAreSet = false;
   std::string backupSourceLinkPath;
   std::string backupDestLinkPath;
   linkPathsAreSet = threadvartk::getThreadKeyNftwPaths(backupSourceLinkPath,
      backupDestLinkPath);
   threadvartk::setThreadKeyNftwPaths(pathFromLink.c_str(), destPath.c_str() );

   // check if the maximum number of symlinks to follow is reached
   if(!threadvartk::testAndIncrementThreadKeyNftwNumFollowedSymlinks() )
   {
      errno = ELOOP;
      log->logErr(__FUNCTION__, "Too many symbolic links were encountered: " +
         std::string(sourcePath) + "; errno: " + System::getErrString(errno) );
      retVal = DEEPER_RETVAL_ERROR;
   }
   else if(S_ISDIR(statNewSourcePath.st_mode) )
   {
      if(unlikely(ignoreDir || doCRC) )
      {
         errno = EISDIR;
         log->logErr(__FUNCTION__, "Symlink points to directory, but the flag "
            "DEEPER_..._FOLLOWSYMLINKS is set and the flag DEEPER_..._SUBDIRS is not set: " +
            pathFromLink + "; errno: " + System::getErrString(errno) );
         retVal = DEEPER_RETVAL_ERROR;
      }
      else
      {
         retVal = copyDir(pathFromLink.c_str(), destPath.c_str(), copyToCache, deleteSource, true);
      }
   }
   else if(S_ISREG(statNewSourcePath.st_mode) )
   {
      retVal = copyFile(pathFromLink.c_str(), destPath.c_str(), &statNewSourcePath, deleteSource,
         doCRC, NULL, crcOutValue);
   }
   else if(S_ISLNK(statNewSourcePath.st_mode) )
   {
      retVal = handleFollowSymlink(pathFromLink.c_str(), destPath, &statNewSourcePath, copyToCache,
         deleteSource, ignoreDir, doCRC, crcOutValue);
   }
   else
   {
      errno = EINVAL;
      log->logErr(__FUNCTION__, "Unexpected typeflag value for path: " + pathFromLink);
      retVal = DEEPER_RETVAL_ERROR;
   }

   // reset the number of followed symlinks
   threadvartk::setThreadKeyNftwNumFollowedSymlinks(backupNumFollowedSymlinks);

   // restore the paths from the previous follow symlink call
   if(linkPathsAreSet)
      threadvartk::setThreadKeyNftwPaths(backupSourceLinkPath.c_str(), backupDestLinkPath.c_str() );
   else
      threadvartk::resetThreadKeyNftwPaths();

   return retVal;
}

/**
 * copies a file from global BeeGFS to the cache BeeGFS
 *
 * NOTE: This function is used by nftw , the interface of this function can not be changed.
 *
 * @param fpath the pathname of the entry relative to the parameter dirpath of the nftw(...) call
 * @param sb is a pointer to the stat structure returned by a call to stat for fpath
 * @param tflag typeflag of the entry which is given to this function by nftw, details in man
 *        page of nftw
 * @param ftwbuf controll flags for nftw, details in man page of nftw
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::nftwCopyFileToCache(const char *fpath, const struct stat *sb, int tflag,
   struct ::FTW *ftwbuf)
{
   Logger* log = Logger::getLogger();
   CacheConfig* cfg = dynamic_cast<CacheConfig*>(PThread::getCurrentThreadApp()->getCommonConfig());

   int retVal = FTW_CONTINUE;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, "CopyFileToCache", "Path: " + std::string(fpath) );
#endif

   std::string sourcePath(fpath);
   std::string destPath(fpath);

   bool deleteSource = threadvartk::getThreadKeyNftwFlushDiscard();
   bool followSymlink = threadvartk::getThreadKeyNftwFollowSymlink();
   bool isLinkPath = false;

   if(followSymlink)
   {
      std::string sourceLinkPath;
      std::string destLinkPath;

      if(threadvartk::getThreadKeyNftwPaths(sourceLinkPath, destLinkPath) )
      {
         if(unlikely(!cachepathtk::replaceRootPath(destPath, sourceLinkPath, destLinkPath, true,
            true, log) ) )
         {
            errno = ENOENT;
            return FTW_STOP;
         }
         isLinkPath = true;
      }
   }

   if(!isLinkPath)
   {
      bool funcRetVal = cachepathtk::globalPathToCachePath(destPath, cfg, log);
      if(unlikely(!funcRetVal) )
      {
         errno = ENOENT;
         return FTW_STOP;
      }
   }

   retVal = nftwHandleFlags(sourcePath, destPath, sb, tflag, true, deleteSource, followSymlink);

   return retVal;
}

/**
 * copies a file from cache BeeGFS to the global BeeGFS
 *
 * NOTE: This function is used by nftw , the interface of this function can not be changed.
 *
 * @param fpath the pathname of the entry relative to the parameter dirpath of the nftw(...) call
 * @param sb is a pointer to the stat structure returned by a call to stat for fpath
 * @param tflag typeflag of the entry which is given to this function by nftw, details in man
 *        page of nftw
 * @param ftwbuf controll flags for nftw, details in man page of nftw
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::nftwCopyFileToGlobal(const char *fpath, const struct stat *sb, int tflag,
   struct ::FTW *ftwbuf)
{
   Logger* log = Logger::getLogger();
   CacheConfig* cfg = dynamic_cast<CacheConfig*>(PThread::getCurrentThreadApp()->getCommonConfig());

   int retVal = FTW_CONTINUE;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, "CopyFileToGlobal", "Path: " + std::string(fpath) );
#endif

   std::string sourcePath(fpath);
   std::string destPath(fpath);

   bool deleteSource = threadvartk::getThreadKeyNftwFlushDiscard();
   bool followSymlink = threadvartk::getThreadKeyNftwFollowSymlink();
   bool isLinkPath = false;

   if(followSymlink)
   {
      std::string sourceLinkPath;
      std::string destLinkPath;

      if(threadvartk::getThreadKeyNftwPaths(sourceLinkPath, destLinkPath) )
      {
         if(unlikely(!cachepathtk::replaceRootPath(destPath, sourceLinkPath, destLinkPath, true,
            false, log) ) )
         {
            errno = ENOENT;
            return FTW_STOP;
         }
         isLinkPath = true;
      }
   }

   if(!isLinkPath)
   {
      bool funcRetVal = cachepathtk::cachePathToGlobalPath(destPath, cfg, log);
      if(unlikely(!funcRetVal) )
      {
         errno = ENOENT;
         return FTW_STOP;
      }
   }

   retVal = nftwHandleFlags(sourcePath, destPath, sb, tflag, false, deleteSource, followSymlink);

   return retVal;
}

/**
 * handles the nftw type flags
 *
 * NOTE: this function is called by the nftw functions
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 * @param sb is a pointer to the stat structure returned by a call to stat for the entry which
 *        was given to to a function by nftw, details in man page of nftw
 * @param tflag typeflag of the entry which was given to a function by nftw, details in man page
 *        of nftw
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache
 *        BeeGFS, false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the file/directory should be deleted after the file/directory was
 *        copied
 * @param followSymlink true if the destination file or directory shoulb be copied and do not
 *        create symbolic links when a symbolic link was found
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::nftwHandleFlags(std::string sourcePath, std::string destPath,
   const struct stat *sb, int tflag, bool copyToCache, bool deleteSource, bool followSymlink)
{
   Logger* log = Logger::getLogger();

   int retValInternal = FTW_CONTINUE;

   if(tflag == FTW_F)
   {
#ifdef BEEGFS_DEBUG
      log->log(Log_DEBUG, "handleFlags", "Source path: " + sourcePath + "; type: file");
#endif

      retValInternal = filesystemtk::copyFile(sourcePath.c_str(), destPath.c_str(), sb,
         deleteSource, false, NULL, NULL);
      if(retValInternal == DEEPER_RETVAL_ERROR)
      {
         log->logErr("handleFlags", "Could not copy file from " + sourcePath + " to " + destPath +
            ". errno: " + System::getErrString(errno) );
         return FTW_STOP;
      }
   }
   else if( (tflag == FTW_D) || (tflag == FTW_DP) )
   {
#ifdef BEEGFS_DEBUG
      log->log(Log_DEBUG, "handleFlags", "Source path: " + sourcePath + "; type: directory");
#endif

      retValInternal = mkdir(destPath.c_str(), sb->st_mode);
      if( (retValInternal == DEEPER_RETVAL_ERROR) && (errno != EEXIST) )
      {
         log->logErr("handleFlags", "Could not create directory: " + destPath +
            "; errno: " + System::getErrString(errno) );

         return FTW_STOP;
      }

      retValInternal = chown(destPath.c_str(), sb->st_uid, sb->st_gid);
      if( (retValInternal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
      { // EPERM happens when the process is not running as root, this can be ignored
         log->logErr("handleFlags", "Could not change owner of directory: " + destPath +
            "; errno: " + System::getErrString(errno) );

         return FTW_STOP;
      }

      /**
       * the flag deleteSource need to be handled in a second nftw tree walk, because at this
       * point the directories are not empty
       */
   }
   else if(tflag == FTW_SL)
   {
#ifdef BEEGFS_DEBUG
      log->log(Log_DEBUG, "handleFlags", "Source path: " + sourcePath + "; type: link");
#endif
      if(followSymlink)
         retValInternal = filesystemtk::handleFollowSymlink(sourcePath, destPath, sb,
            copyToCache, deleteSource, false, false, NULL);
      else
         retValInternal = filesystemtk::createSymlink(sourcePath.c_str(), destPath.c_str(), sb,
            copyToCache, deleteSource);

      if(retValInternal == DEEPER_RETVAL_ERROR)
         return FTW_STOP;
   }
   else if( (tflag == FTW_DNR) || (tflag == FTW_NS) || (tflag == FTW_SLN) )
   {
      errno = EINVAL;
      log->logErr("handleFlags", "Unexpected typeflag value for path: " + sourcePath);
   }

   return retValInternal;
}

/**
 * deletes files, symlinks and directory
 *
 * NOTE: This function is used by nftw , the interface of this function can not be changed.
 *
 * @param fpath the pathname of the entry relative to the parameter dirpath of the nftw(...) call
 * @param sb is a pointer to the stat structure returned by a call to stat for fpath
 * @param tflag typeflag of the entry which is given to this function by nftw, details in man
 *        page of nftw
 * @param ftwbuf controll flags for nftw, details in man page of nftw
 * @return 0 on success, -1 and errno set in case of error.
 */
int filesystemtk::nftwDelete(const char *fpath, const struct stat *sb, int tflag,
   struct ::FTW *ftwbuf)
{
   Logger* log = Logger::getLogger();

   bool followSymlink = threadvartk::getThreadKeyNftwFollowSymlink();

   if( (tflag == FTW_D) || (tflag == FTW_DP) )
   {
#ifdef BEEGFS_DEBUG
      log->log(Log_DEBUG, "delete", "Source path: " + std::string(fpath) + "; type: directory");
#endif
      if(rmdir(fpath) )
      {
         log->logErr("delete", "Could not delete directory after copy. path: " +
            std::string(fpath) + "; errno: " + System::getErrString(errno) );
         return FTW_STOP;
      }
   }
   else if( (tflag == FTW_F) || (tflag == FTW_SL) )
   {
#ifdef BEEGFS_DEBUG
      log->log(Log_DEBUG, "delete", "Source path: " + std::string(fpath) +
         "; type: file or symlink");
#endif
      if(unlikely(!followSymlink) )
      {
         log->logErr("delete", "Found a file or symlink " + std::string(fpath) +
            ", but all files and symlinks should be deleted.");
         return FTW_STOP;
      }

      if(unlink(fpath) )
      {
         log->logErr("delete", "Could not delete file or symlink. path: " + std::string(fpath) );
         return FTW_STOP;
      }
   }
   else if( (tflag == FTW_DNR) || (tflag == FTW_NS) || (tflag == FTW_SLN) )
   {
      log->logErr("delete", "Unexpected typeflag value.");
      return FTW_STOP;
   }

   return FTW_CONTINUE;
}
