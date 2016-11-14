#include <common/threading/SafeRWLock.h>
#include <common/toolkit/StringTk.h>
#include <deeper/deeper_cache.h>
#include <toolkit/CachePathTk.h>
#include "DeeperCache.h"


#include <ftw.h>
#include <libgen.h>
#include <sys/stat.h>
#include <zlib.h>


#ifndef BEEGFS_VERSION
   #error BEEGFS_VERSION undefined
#endif

#if !defined(BEEGFS_VERSION_CODE) || (BEEGFS_VERSION_CODE == 0)
   #error BEEGFS_VERSION_CODE undefined
#endif


static pthread_key_t nftwFlushDiscard;
static pthread_once_t nftwKeyOnceFlushDiscard = PTHREAD_ONCE_INIT;

static pthread_key_t nftwFollowSymlink;
static pthread_once_t nftwKeyOnceFollowSymlink = PTHREAD_ONCE_INIT;

static pthread_key_t nftwNumFollowedSymlinks;
static pthread_once_t nftwKeyOnceNumFollowedSymlinks = PTHREAD_ONCE_INIT;

static pthread_key_t nftwDestPath;
static pthread_once_t nftwKeyOnceDestPath = PTHREAD_ONCE_INIT;

static pthread_key_t nftwSourcePath;
static pthread_once_t nftwKeyOnceSourcePath = PTHREAD_ONCE_INIT;


const unsigned NFTW_MAX_OPEN_FD        = 100;               // max open file descriptors for NFTW
const size_t BUFFER_SIZE               = (1024 * 1024 * 2); // buffer size 2 MB
const int MAX_NUM_FOLLOWED_SYMLINKS    = 20;                // max number of symlinks to follow


// static data member for singleton which is visible to external programs which use this lib
DeeperCache* DeeperCache::cacheInstance = NULL;

/**
 * constructor
 */
DeeperCache::DeeperCache()
{
   char* argvLib[1];
   argvLib[0] = (char*) malloc(20);
   strcpy(argvLib[0], "beegfs_deeper_lib");
   int argcLib = 1;

   this->cfg = new Config(argcLib, argvLib);
   this->logger = new Logger(this->cfg);
   this->logger->log(Log_DEBUG, "beegfs-deeper-lib", std::string("BeeGFS DEEP-ER Cache library "
      "- Version: ") + BEEGFS_VERSION + " - Cache ID: " + StringTk::uint64ToStr(
      this->cfg->getSysCacheID() ) + " - cache FS path: " + this->cfg->getSysMountPointCache() +
      " - global FS Path: " + this->cfg->getSysMountPointGlobal() );

   free(argvLib[0]);
}

/**
 * destructor
 */
DeeperCache::~DeeperCache()
{
   SAFE_DELETE(logger);
   SAFE_DELETE(cfg);
}

/**
 * Initialize or get a deeper-cache object as a singleton
 */
DeeperCache* DeeperCache::getInstance()
{
   if(!cacheInstance)
      cacheInstance = new DeeperCache();

   return cacheInstance;
}

/**
 * cleanup method to call the destructor in the lib
 */
void DeeperCache::cleanup()
{
   SAFE_DELETE(cacheInstance);
}


/**
 * Create a new directory on the cache layer of the current cache domain.
 *
 * @param path path and name of new directory.
 * @param mode permission bits, similar to POSIX mkdir (S_IRWXU etc.).
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_mkdir(const char *path, mode_t mode)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string newPath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + newPath);
#endif

   if(CachePathTk::globalPathToCachePath(newPath, this->cfg, this->logger) )
      retVal = mkdir(newPath.c_str(), mode);
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Remove a directory from the cache layer.
 *
 * @param path path and name of directory, which should be removed.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_rmdir(const char *path)
{
   int retVal = DEEPER_RETVAL_ERROR;
   std::string newPath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + newPath);
#endif

   if(CachePathTk::globalPathToCachePath(newPath, this->cfg, this->logger) )
      retVal = rmdir(newPath.c_str() );
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Open a directory stream for a directory on the cache layer of the current cache domain and
 * position the stream at the first directory entry.
 *
 * @param name path and name of directory on cache layer.
 * @return pointer to directory stream, NULL and errno set in case of error.
 */
DIR* DeeperCache::cache_opendir(const char *name)
{
   DIR* retVal = NULL;
   std::string newName(name);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + newName);
#endif

   if(CachePathTk::globalPathToCachePath(newName, this->cfg, this->logger) )
      retVal = opendir(newName.c_str() );
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Close directory stream.
 *
 * @param dirp directory stream, which should be closed.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_closedir(DIR *dirp)
{
   return closedir(dirp);
}

/**
 * Open (and possibly create) a file on the cache layer.
 *
 * Any following write()/read() on the returned file descriptor will write/read data to/from the
 * cache layer. Data on the cache layer will be visible only to those nodes that are part of the
 * same cache domain.
 *
 * @param path path and name of file, which should be opened.
 * @param oflag access mode flags, similar to POSIX open (O_WRONLY, O_CREAT, etc).
 * @param mode the permissions of the file, similar to POSIX open. When oflag contains a file
 *        creation flag (O_CREAT, O_EXCL, O_NOCTTY, and O_TRUNC) the mode flag is required in other
 *        cases this flag is ignored.
 * @param deeper_open_flags zero or a combination of the following flags:
 *        DEEPER_OPEN_FLUSHONCLOSE to automatically flush written data to global
 *           storage when the file is closed, asynchronously;
 *        DEEPER_OPEN_FLUSHWAIT to make DEEPER_OPEN_FLUSHONCLOSE a synchronous operation, which
 *        means the close operation will only return after flushing is complete;
 *        DEEPER_OPEN_DISCARD to remove the file from the cache layer after it has been closed.
 * @return file descriptor as non-negative integer on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_open(const char* path, int oflag, mode_t mode, int deeper_open_flags)
{
   int retFD = DEEPER_RETVAL_ERROR;
   std::string newName(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + newName);
#endif

   if(!CachePathTk::globalPathToCachePath(newName, this->cfg, this->logger) )
   {
      errno = EINVAL;
      return retFD;
   }

   retFD = open(newName.c_str(), oflag, mode);

   if( (retFD >= 0) && (deeper_open_flags != DEEPER_OPEN_NONE ) )
      addEntryToSessionMap(retFD, deeper_open_flags, path);

   return retFD;
}

/**
 * Close a file.
 *
 * @param fildes file descriptor of open file.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_close(int fildes)
{
   int retVal = DEEPER_RETVAL_ERROR;
   bool doFlush = false;

   DeeperCacheSession session(fildes);
   getAndRemoveEntryFromSessionMap(session);

   int deeperFlushFlags = DEEPER_FLUSH_NONE;

   if( (session.getDeeperOpenFlags() & DEEPER_OPEN_FLUSHONCLOSE) != 0 )
      doFlush = true;

   if( (session.getDeeperOpenFlags() & DEEPER_OPEN_FLUSHWAIT) != 0 )
   {
      doFlush = true;
      deeperFlushFlags |= DEEPER_FLUSH_WAIT;
   }

   if(session.getDeeperOpenFlags() & DEEPER_OPEN_FLUSHFOLLOWSYMLINKS)
   {
      doFlush = true;
      deeperFlushFlags |= DEEPER_FLUSH_FOLLOWSYMLINKS;
   }

   if( (session.getDeeperOpenFlags() & DEEPER_OPEN_DISCARD) != 0 )
      deeperFlushFlags |= DEEPER_FLUSH_DISCARD;

   retVal = close(fildes);

   if(doFlush)
      retVal = cache_flush(session.getPath().c_str(), deeperFlushFlags);

   if( (!doFlush) && ( (session.getDeeperOpenFlags() & DEEPER_OPEN_DISCARD) != 0) )
   {
      std::string cachePath(session.getPath() );
      if(!CachePathTk::globalPathToCachePath(cachePath, this->cfg, this->logger) )
         retVal = DEEPER_RETVAL_ERROR;
      else
         retVal = unlink(cachePath.c_str() );
   }

   return retVal;
}

/**
 * Prefetch a file or directory (including contained files) from global storage to the current
 * cache domain of the cache layer, asynchronously.
 * Contents of existing files with the same name on the cache layer will be overwritten.
 *
 * @param path path to file or directory, which should be prefetched.
 * @param deeper_prefetch_flags zero or a combination of the following flags:
 *        DEEPER_PREFETCH_SUBDIRS to recursively copy all subdirs, if given path leads to a
 *           directory.
 *        DEEPER_PREFETCH_WAIT to make this a synchronous operation.
 *        DEEPER_PREFETCH_FOLLOWSYMLINKS to copy the destination file or directory and do not
 *           create symbolic links when a symbolic link was found.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_prefetch(const char* path, int deeper_prefetch_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + cachePath);
#endif

   if(!CachePathTk::globalPathToCachePath(cachePath, this->cfg, this->logger) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(deeper_prefetch_flags & DEEPER_PREFETCH_SUBDIRS)
      retVal = handleSubdirectoryFlag(path, cachePath.c_str(), true, false,
         deeper_prefetch_flags & DEEPER_PREFETCH_FOLLOWSYMLINKS);
   else
   {
      if(!CachePathTk::createPath(cachePath, cfg->getSysMountPointGlobal(),
         cfg->getSysMountPointCache(), true, this->logger) )
            return DEEPER_RETVAL_ERROR;

      struct stat statSource;
      int funcError = lstat(path, &statSource);
      if(funcError != 0)
      {
         this->logger->logErr(__FUNCTION__, "Could not stat source file/directory " +
            cachePath + " Errno: " + System::getErrString(errno) );
         return DEEPER_RETVAL_ERROR;
      }

      if(S_ISLNK(statSource.st_mode) )
      {
         if(deeper_prefetch_flags & DEEPER_PREFETCH_FOLLOWSYMLINKS)
         {
            this->setThreadKeyNftwFollowSymlink(true);

            retVal = handleFollowSymlink(path, cachePath.c_str(),  &statSource, true, false, true,
               false, NULL);

            //reset thread variables
            this->setThreadKeyNftwFollowSymlink(false);
            this->resetThreadKeyNftwNumFollowedSymlinks();
         }
         else
            retVal = createSymlink(path, cachePath.c_str(), &statSource, true, false);
      }
      else
      if(S_ISREG(statSource.st_mode) )
         retVal = copyFile(path, cachePath.c_str(), &statSource, false, false, NULL);
      else
      if(S_ISDIR(statSource.st_mode) )
      {
         errno = EISDIR;
         this->logger->logErr(__FUNCTION__, std::string("Given path is an directory but "
            "DEEPER_PREFETCH_SUBDIRS flag is not set: ") + path);
         retVal = DEEPER_RETVAL_ERROR;
      }
      else
      {
         errno = EINVAL;
         this->logger->logErr(__FUNCTION__, std::string("Unsupported file type for path: ") + path);
         retVal = DEEPER_RETVAL_ERROR;
      }
   }

   return retVal;
}

/**
 * Prefetch a file from global storage to the current cache domain of the cache layer,
 * asynchronously. A CRC checksum of the given file is calculated.
 * Contents of existing files with the same name on the cache layer will be overwritten.
 *
 * @param path path to file or directory, which should be prefetched.
 * @param deeper_prefetch_flags zero or a combination of the following flags:
 *        DEEPER_PREFETCH_WAIT to make this a synchronous operation.
 *        DEEPER_PREFETCH_FOLLOWSYMLINKS to copy the destination file or directory and do not
 *           create symbolic links when a symbolic link was found;
 * @param outChecksum The checksum of the file.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_prefetch_crc(const char* path, int deeper_prefetch_flags,
   unsigned long* outChecksum)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + cachePath);
#endif

   if(!CachePathTk::globalPathToCachePath(cachePath, this->cfg, this->logger) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(!CachePathTk::createPath(cachePath, cfg->getSysMountPointGlobal(),
      cfg->getSysMountPointCache(), true, this->logger) )
         return DEEPER_RETVAL_ERROR;

   struct stat statSource;
   int funcError = lstat(path, &statSource);
   if(funcError != 0)
   {
      this->logger->logErr(__FUNCTION__, "Could not stat source file/directory " +
         cachePath + " Errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   if(S_ISREG(statSource.st_mode) )
      retVal = copyFile(path, cachePath.c_str(), &statSource, false, true, outChecksum);
   else
   if(S_ISLNK(statSource.st_mode) )
   {
      if(deeper_prefetch_flags & DEEPER_PREFETCH_FOLLOWSYMLINKS)
      {
         this->setThreadKeyNftwFollowSymlink(true);

         retVal = handleFollowSymlink(path, cachePath.c_str(),  &statSource, true, false, true,
            true, outChecksum);

         //reset thread variables
         this->setThreadKeyNftwFollowSymlink(false);
         this->resetThreadKeyNftwNumFollowedSymlinks();
      }
      else
      {
         errno = ENOLINK;
         this->logger->logErr(__FUNCTION__, std::string("Given path is a symlink. CRC checksum "
            "calculation requires a file or the DEEPER_PREFETCH_FOLLOWSYMLINKS flag: ") + path);
         retVal = DEEPER_RETVAL_ERROR;
      }
   }
   else
   if(S_ISDIR(statSource.st_mode) )
   {
      errno = EISDIR;
      this->logger->logErr(__FUNCTION__, std::string("Given path is an directory. CRC checksum "
         "calculation doesn't support directories: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   {
      errno = EINVAL;
      this->logger->logErr(__FUNCTION__, std::string("Unsupported file type for path: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }

   return retVal;
}

/**
 * Prefetch a file similar to deeper_cache_prefetch(), but prefetch only a certain range, not the
 * whole file.
 *
 * @param path path to file, which should be prefetched.
 * @param pos start position (offset) of the byte range that should be flushed.
 * @param num_bytes number of bytes from pos that should be flushed.
 * @param deeper_prefetch_flags zero or the following flag:
  *        DEEPER_PREFETCH_WAIT to make this a synchronous operation.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_prefetch_range(const char* path, off_t start_pos, size_t num_bytes,
   int deeper_prefetch_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + cachePath);
#endif

   if(!CachePathTk::globalPathToCachePath(cachePath, this->cfg, this->logger) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(!CachePathTk::createPath(cachePath, cfg->getSysMountPointGlobal(),
      cfg->getSysMountPointCache(), true, this->logger) )
         return DEEPER_RETVAL_ERROR;

   retVal = copyFileRange(path, cachePath.c_str(), NULL, &start_pos, num_bytes);

   return retVal;
}

/**
 * Wait for an ongoing prefetch operation from deeper_cache_prefetch[_range]() to complete.
 *
 * @param path path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags zero or a combination of the following flags:
 *        DEEPER_PREFETCH_SUBDIRS to recursively wait contents of all subdirs, if given path leads
 *           to a directory.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_prefetch_wait(const char* path, int deeper_prefetch_flags)
{
   // not implemented in the synchronous implementation
   return DEEPER_RETVAL_SUCCESS;
}

/**
 * Flush a file from the current cache domain to global storage, asynchronously.
 * Contents of an existing file with the same name on global storage will be overwritten.
 *
 * @param path path to file, which should be flushed.
 * @param deeper_flush_flags zero or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT to make this a synchronous operation, which means return only after
 *           flushing is complete;
 *        DEEPER_FLUSH_SUBDIRS to recursively copy all subdirs, if given path leads to a
 *           directory;
 *        DEEPER_FLUSH_DISCARD to remove the file from the cache layer after it has been flushed.
 *        DEEPER_FLUSH_FOLLOWSYMLINKS to copy the destination file or directory and do not create
 *           symbolic links when a symbolic link was found;
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_flush(const char* path, int deeper_flush_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + cachePath);
#endif

   if(!CachePathTk::globalPathToCachePath(cachePath, this->cfg, this->logger) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(deeper_flush_flags & DEEPER_FLUSH_SUBDIRS)
      retVal = handleSubdirectoryFlag(cachePath.c_str(), path, false,
         deeper_flush_flags & DEEPER_FLUSH_DISCARD,
         deeper_flush_flags & DEEPER_FLUSH_FOLLOWSYMLINKS);
   else
   {
      if(!CachePathTk::createPath(path, cfg->getSysMountPointCache(), cfg->getSysMountPointGlobal(),
            true, this->logger) )
         return DEEPER_RETVAL_ERROR;

      struct stat statSource;
      int funcError = lstat(cachePath.c_str(), &statSource);
      if(funcError != 0)
      {
         this->logger->logErr(__FUNCTION__, "Could not stat source file/directory " +
            cachePath + " Errno: " + System::getErrString(errno) );
         return DEEPER_RETVAL_ERROR;
      }

      if(S_ISLNK(statSource.st_mode) )
      {
         if(deeper_flush_flags & DEEPER_FLUSH_FOLLOWSYMLINKS)
         {
            this->setThreadKeyNftwFollowSymlink(true);

            retVal = handleFollowSymlink(cachePath.c_str(), path, &statSource, false,
               deeper_flush_flags & DEEPER_FLUSH_DISCARD, true, false, NULL);

            //reset thread variables
            this->setThreadKeyNftwFollowSymlink(false);
            this->resetThreadKeyNftwNumFollowedSymlinks();
         }
         else
            retVal = createSymlink(cachePath.c_str(), path, &statSource, false,
               deeper_flush_flags & DEEPER_FLUSH_DISCARD);
      }
      else
      if(S_ISREG(statSource.st_mode) )
         retVal = copyFile(cachePath.c_str(), path, &statSource,
            deeper_flush_flags & DEEPER_FLUSH_DISCARD, false, NULL);
      else
      if(S_ISDIR(statSource.st_mode) )
      {
         errno = EISDIR;
         this->logger->logErr(__FUNCTION__, std::string("Given path is an directory but "
            "DEEPER_FLUSH_SUBDIRS flag is not set: ") + cachePath);
         retVal = DEEPER_RETVAL_ERROR;
      }
      else
      {
         errno = EINVAL;
         this->logger->logErr(__FUNCTION__, std::string("Unsupported file type for path: ") + path);
         retVal = DEEPER_RETVAL_ERROR;
      }
   }

   return retVal;
}

/**
 * Flush a file from the current cache domain to global storage, asynchronously. A CRC checksum of
 * the given file is calculated.
 * Contents of an existing file with the same name on global storage will be overwritten.
 *
 * @param path path to file, which should be flushed.
 * @param deeper_flush_flags zero or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT to make this a synchronous operation, which means return only after
 *           flushing is complete;
 *        DEEPER_FLUSH_DISCARD to remove the file from the cache layer after it has been flushed.
 *        DEEPER_FLUSH_FOLLOWSYMLINKS to copy the destination file or directory and do not create
 *           symbolic links when a symbolic link was found;
 * @param outChecksum The checksum of the file.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_flush_crc(const char* path, int deeper_flush_flags,
   unsigned long* outChecksum)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + cachePath);
#endif

   if(!CachePathTk::globalPathToCachePath(cachePath, this->cfg, this->logger) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(!CachePathTk::createPath(path, cfg->getSysMountPointCache(), cfg->getSysMountPointGlobal(),
         true, this->logger) )
      return DEEPER_RETVAL_ERROR;

   struct stat statSource;
   int funcError = lstat(cachePath.c_str(), &statSource);
   if(funcError != 0)
   {
      this->logger->logErr(__FUNCTION__, "Could not stat source file/directory " +
         cachePath + " Errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   if(S_ISREG(statSource.st_mode) )
      retVal = copyFile(cachePath.c_str(), path, &statSource,
         deeper_flush_flags & DEEPER_FLUSH_DISCARD, true, outChecksum);
   else
   if(S_ISLNK(statSource.st_mode) )
   {
      if(deeper_flush_flags & DEEPER_FLUSH_FOLLOWSYMLINKS)
      {
         this->setThreadKeyNftwFollowSymlink(true);

         retVal = handleFollowSymlink(cachePath.c_str(), path, &statSource, false,
            deeper_flush_flags & DEEPER_FLUSH_DISCARD, true, true, outChecksum);

         //reset thread variables
         this->setThreadKeyNftwFollowSymlink(false);
         this->resetThreadKeyNftwNumFollowedSymlinks();
      }
      else
      {
         errno = ENOLINK;
         this->logger->logErr(__FUNCTION__, std::string("Given path is a symlink. CRC checksum "
            "calculation requires a file or the DEEPER_FLUSH_FOLLOWSYMLINKS flag: ") + path);
         retVal = DEEPER_RETVAL_ERROR;
      }
   }
   else
   if(S_ISDIR(statSource.st_mode) )
   {
      errno = EISDIR;
      this->logger->logErr(__FUNCTION__, std::string("Given path is an directory. CRC checksum "
         "calculation doesn't support directories: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   {
      errno = EINVAL;
      this->logger->logErr(__FUNCTION__, std::string("Unsupported file type for path: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }

   return retVal;
}

/**
 * Flush a file similar to deeper_cache_flush(), but flush only a certain range, not the whole
 * file.
 *
 * @param path path to file, which should be flushed.
 * @param pos start position (offset) of the byte range that should be flushed.
 * @param num_bytes number of bytes from pos that should be flushed.
 * @param deeper_flush_flags zero or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT to make this a synchronous operation.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_flush_range(const char* path, off_t start_pos, size_t num_bytes,
   int deeper_flush_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + cachePath);
#endif

   if(!CachePathTk::globalPathToCachePath(cachePath, this->cfg, this->logger) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(!CachePathTk::createPath(path, cfg->getSysMountPointCache(), cfg->getSysMountPointGlobal(),
      true, this->logger) )
         return DEEPER_RETVAL_ERROR;

   retVal = copyFileRange(cachePath.c_str(), path, NULL, &start_pos, num_bytes);

   return retVal;
}

/**
 * Wait for an ongoing flush operation from deeper_cache_flush[_range]() to complete.
 *
 * @param path path to file, which has been submitted for flush.
 * @param deeper_flush_flags zero or a combination of the following flags:
 *        DEEPER_FLUSH_SUBDIRS to recursively wait contents of all subdirs, if given path leads
 *           to a directory.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_flush_wait(const char* path, int deeper_flush_flags)
{
   // not implemented in the synchronous implementation
   return DEEPER_RETVAL_SUCCESS;
}

/**
 * Return the stat information of a file or directory of the cache domain.
 *
 * @param path to a file or directory on the global file system
 * @param out_stat_data the stat information of the file or directory
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_stat(const char *path, struct stat *out_stat_data)
{
   int retVal = DEEPER_RETVAL_ERROR;
   std::string newPath(path);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + newPath);
#endif

   if(CachePathTk::globalPathToCachePath(newPath, this->cfg, this->logger) )
      retVal = stat(newPath.c_str(), out_stat_data);
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Return a unique identifier for the current cache domain.
 *
 * @param out_cache_id pointer to a buffer in which the ID of the current cache domain will be
 *        stored on success.
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::cache_id(const char* path, uint64_t* out_cache_id)
{
   std::string newPath(path);

   CachePathTk::preparePaths(newPath);

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "path: " + newPath);
#endif

   if(newPath[0] != '/')
   {
      if(CachePathTk::makePathAbsolute(newPath, this->logger) )
         return DEEPER_RETVAL_ERROR;
   }

   if(!CachePathTk::isRootPath(newPath, cfg->getSysMountPointGlobal() ) )
   {
      this->logger->logErr(__FUNCTION__, "The given path doesn't point to a global BeeGFS: " +
         std::string(path) );
      errno = EINVAL;
      return DEEPER_RETVAL_ERROR;
   }

   *out_cache_id = this->cfg->getSysCacheID();

   return DEEPER_RETVAL_SUCCESS;
}



/**
 * add file descriptor with the deeper_open_flags to the map
 *
 * @param fd the fd of the file
 * @param deeper_open_flags the deeper_open_flags which was used during the open
 * @param path the path of the file
 */
void DeeperCache::addEntryToSessionMap(int fd, int deeper_open_flags, std::string path)
{
   SafeMutexLock lock(&fdMapMutex);                         // W R I T E L O C K

   DeeperCacheSession session(fd, deeper_open_flags, path);
   std::pair<DeeperCacheSessionMapIter, bool> pair = this->sessionMap.insert(
      DeeperCacheSessionMapVal(fd, session) );

   if(!(pair.second) )
      this->logger->logErr(__FUNCTION__, "Could not add FD to session map: " +
         StringTk::intToStr(fd) + " (" + path + ")");

   lock.unlock();                                           // U N L O C K
}

/**
 * get the DeeperCacheSession for a given file descriptor and remove the session from the map
 *
 * @param inOutSession the session with the searched FD and after calling this method it contains
 *                     all required information
 */
void DeeperCache::getAndRemoveEntryFromSessionMap(DeeperCacheSession& inOutSession)
{
   SafeMutexLock lock(&fdMapMutex);                         // W R I T E L O C K

   DeeperCacheSessionMapIter iter = this->sessionMap.find(inOutSession.getFD() );
   if(iter != this->sessionMap.end() )
   {
      inOutSession.setDeeperOpenFlags(iter->second.getDeeperOpenFlags() );
      inOutSession.setPath(iter->second.getPath() );

      this->sessionMap.erase(iter);
   }
   else
      this->logger->log(Log_DEBUG, __FUNCTION__, "Could not find FD in session map: " +
         StringTk::intToStr(inOutSession.getFD() ) );

   lock.unlock();                                           // U N L O C K
}



/**
 * copy a file, set the mode and owner of the source file to the destination file
 *
 * @param sourcePath the path to the source file
 * @param destPath the path to the destination file
 * @param statSource the stat of the source file, if this pointer is NULL this function stats the
 *        source file
 * @param deleteSource true if the file should be deleted after the file was copied
 * @param doCRC true if a CRC checksum should be calculated
 * @param crcOutValue out value for the checksum if a checksum should be calculated
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::copyFile(const char* sourcePath, const char* destPath,
   const struct stat* statSource, bool deleteSource, bool doCRC, unsigned long* crcOutValue)
{
   int retVal = DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "source path: " + std::string(sourcePath) +
      " - dest path: " + destPath);
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
      this->logger->logErr(__FUNCTION__, "Could not allocate memory to copy the file: " +
         std::string(sourcePath) + " Errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   int source = open(sourcePath, openFlagSource, 0);
   if(source == DEEPER_RETVAL_ERROR)
   {
      this->logger->logErr(__FUNCTION__, "Could not open source file: " +
         std::string(sourcePath) + " Errno: " + System::getErrString(errno) );

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

         this->logger->logErr(__FUNCTION__, "Could not stat source file: " +
            std::string(sourcePath) + " Errno: " + System::getErrString(saveErrno) );

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

      this->logger->logErr(__FUNCTION__, "Could not open destination file: " +
         std::string(destPath) + " Errno: " + System::getErrString(saveErrno) );

      goto close_source;
   }

   if(doCRC && (crcOutValue != NULL) )
      *crcOutValue = crc32(0L, Z_NULL, 0); // init checksum

   while ( (readSize = read(source, buf, BUFFER_SIZE) ) > 0)
   {
      if(doCRC)
         *crcOutValue = crc32(*crcOutValue, buf, readSize);

      writeSize = write(dest, buf, readSize);

      if(unlikely(writeSize != readSize) )
      {
         saveErrno = errno;
         error = true;

         this->logger->logErr(__FUNCTION__, "Could not copy file: " + std::string(sourcePath)
            + " to " + std::string(destPath) + " Errno: " + System::getErrString(saveErrno) );

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

      this->logger->logErr(__FUNCTION__, "Could not set owner of file: " +
         std::string(destPath) + " Errno: " + System::getErrString(saveErrno) );
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
   else
   if(deleteSource)
   {
      if(unlink(sourcePath) )
         this->logger->logErr(__FUNCTION__, "Could not delete file after copy " +
            std::string(sourcePath) + " Errno: " + System::getErrString(errno) );
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
 * @param numBytes the number of bytes to copy starting by the offset, required if copyRange is true
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::copyFileRange(const char* sourcePath, const char* destPath,
   const struct stat* statSource, off_t* offset, size_t numBytes)
{
   int retVal = DEEPER_RETVAL_ERROR;
   off_t retOffset = 0;

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "source path: " + std::string(sourcePath) +
      " - dest path: " + destPath);
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
      this->logger->logErr(__FUNCTION__, "Could not allocate memory to copy the file: " +
         std::string(sourcePath) + " Errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   int source = open(sourcePath, openFlagsSource, 0);
   if(source == DEEPER_RETVAL_ERROR)
   {
      this->logger->logErr(__FUNCTION__, "Could not open source file: " +
         std::string(sourcePath) + " Errno: " + System::getErrString(errno) );

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

        this->logger->logErr(__FUNCTION__, "Could not stat source file: " +
           std::string(sourcePath) + " Errno: " + System::getErrString(saveErrno) );

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

     this->logger->logErr(__FUNCTION__, "Could not open destination file: " +
        std::string(destPath) + " Errno: " + System::getErrString(saveErrno) );

     goto close_source;
   }

   retOffset = lseek(source, *offset, SEEK_SET);
   if(retOffset != *offset)
   {
      saveErrno = errno;
      error = true;

      this->logger->logErr(__FUNCTION__, "Could not seek in source file: " +
         std::string(sourcePath) + " Errno: " + System::getErrString(saveErrno) );

      goto close_dest;
   }

   retOffset = lseek(dest, *offset, SEEK_SET);
   if(retOffset != *offset)
   {
      saveErrno = errno;
      error = true;

      this->logger->logErr(__FUNCTION__, "Could not seek in destination file: " +
         std::string(destPath) + " Errno: " + System::getErrString(saveErrno) );

      goto close_dest;
   }

   nextReadSize = std::min(dataToRead, BUFFER_SIZE);
   while(nextReadSize > 0)
   {
      readSize = read(source, buf, nextReadSize);

      if(readSize == -1)
      {
         saveErrno = errno;
         error = true;

         this->logger->logErr(__FUNCTION__, "Could not copy file: " +
            std::string(sourcePath) + " to " + std::string(destPath) + " Errno: " +
            System::getErrString(saveErrno) );

         goto close_dest;
      }

      writeSize = write(dest, buf, readSize);

      if(unlikely(writeSize != readSize) )
      {
         saveErrno = errno;
         error = true;

         this->logger->logErr(__FUNCTION__, "Could not read data from file: " +
            std::string(sourcePath) + " Errno: " + System::getErrString(saveErrno) );

         goto close_dest;
      }

      dataToRead -= readSize;
      nextReadSize = std::min(dataToRead, BUFFER_SIZE);
   }

   if(!statSource)
      retVal = fchown(dest, newStatSource.st_uid, newStatSource.st_gid);
   else
      retVal = fchown(dest, statSource->st_uid, statSource->st_gid);

   if( (retVal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
   {
      saveErrno = errno;
      error = true;

      this->logger->logErr(__FUNCTION__, "Could not set owner of file: " +
         std::string(destPath) + " Errno: " + System::getErrString(saveErrno) );
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
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache BeeGFS,
 *        false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the directory should be deleted after the directory was copied, but
 *        only files are deleted, directories must be deleted in a second nftw run, if followSymlink
 *        is set nothing will be deleted, the second nftw run must also delete the files
 * @param followSymlink true if the destination file or directory should be copied and do not create
 *        symbolic links when a symbolic link was found
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::copyDir(const char* source, const char* dest, bool copyToCache, bool deleteSource,
   bool followSymlink)
{
   int retVal = DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "source path: " + std::string(source) +
      " - dest path: " + dest);
#endif

   Config* cfg = this->getConfig();

   /**
    * used nftw flags copy directories
    * FTW_ACTIONRETVAL  handles the return value from the fn(), fn() must return special values
    * FTW_PHYS          do not follow symbolic links
    * FTW_MOUNT         stay within the same file system
    */
   const int nftwFlagsCopy = FTW_ACTIONRETVAL | FTW_PHYS | FTW_MOUNT;

   const std::string& mountOfSource(copyToCache ? cfg->getSysMountPointGlobal() :
      cfg->getSysMountPointCache());
   const std::string& mountOfDest(copyToCache ? cfg->getSysMountPointCache() :
      cfg->getSysMountPointGlobal());

   struct stat statSource;
   int funcError = lstat(source, &statSource);
   if(funcError != 0)
   {
      this->logger->logErr(__FUNCTION__, "Could not stat source file/directory " +
         std::string(source) + " Errno: " + System::getErrString(errno) );
      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   if(S_ISREG(statSource.st_mode) )
   {
      if(!CachePathTk::createPath(dest, mountOfSource, mountOfDest, true, this->logger) )
         return DEEPER_RETVAL_ERROR;

      retVal = copyFile(source, dest, &statSource, deleteSource, false, NULL);
   }
   else
   if(S_ISLNK(statSource.st_mode) )
   {
      if(!CachePathTk::createPath(dest, mountOfSource, mountOfDest, true, this->logger) )
         return DEEPER_RETVAL_ERROR;

      if(followSymlink)
         retVal= handleFollowSymlink(source, dest, &statSource, copyToCache, deleteSource, false,
            false, NULL);
      else
         retVal = createSymlink(source, dest, &statSource, copyToCache, deleteSource);
   }
   else
   {
      if(!CachePathTk::createPath(dest, mountOfSource, mountOfDest, false, this->logger) )
         return retVal;

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
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache BeeGFS,
 *        false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the file/directory should be deleted after the file/directory was
 *        copied
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::createSymlink(const char* sourcePath, const char* destPath,
   const struct stat* statSource, bool copyToCache, bool deleteSource)
{
   int retVal = DEEPER_RETVAL_SUCCESS;
   std::string linkDest;

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "source path: " + std::string(sourcePath) +
      " - dest path: " + destPath );
#endif

   if(!CachePathTk::readLink(sourcePath, statSource, linkDest, this->logger) )
      return DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "link destination: " + linkDest);
#endif

   // only convert absolute paths
   if(linkDest[0] == '/')
   {
      bool retValConvert = true;
      if(copyToCache)
      { // ...but only if the path points to the global FS
         if(CachePathTk::isGlobalPath(linkDest, this->cfg) )
            retValConvert = CachePathTk::globalPathToCachePath(linkDest, this->cfg, this->logger);
      }
      else
      { // ...but only if the path points to the cache FS
         if(CachePathTk::isCachePath(linkDest, this->cfg) )
            retValConvert = CachePathTk::cachePathToGlobalPath(linkDest, this->cfg, this->logger);
      }

      if(!retValConvert)
         return DEEPER_RETVAL_ERROR;
   }

   retVal = symlink(linkDest.c_str(), destPath );
   if(retVal == DEEPER_RETVAL_ERROR)
   {
      this->logger->logErr("handleFlags", "Could not create symlink " +
         std::string(destPath) + " to " + linkDest + " Errno: " + System::getErrString(errno) );

      return DEEPER_RETVAL_ERROR;
   }

   if(deleteSource)
   {
      if(unlink(sourcePath) )
         this->logger->logErr("handleFlags", "Could not delete symlink after copy " +
            std::string(sourcePath) + " Errno: " + System::getErrString(errno) );
   }

   return DEEPER_RETVAL_SUCCESS;
}

/**
 * copies a directory to an other directory using nftw(), also do cleanup the source directory
 *
 * @param source the path to the source directory
 * @param dest the path to the destination directory
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache BeeGFS,
 *        false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the directory should be deleted after the directory was copied
 * @param followSymlink true if the destination file or directory should be copied and do not create
 *        symbolic links when a symbolic link was found
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::handleSubdirectoryFlag(const char* source, const char* dest, bool copyToCache,
   bool deleteSource, bool followSymlink)
{
#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "source path: " + std::string(source) +
      " - dest path: " + dest );
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
      this->setThreadKeyNftwFollowSymlink(true);

   // set the discard thread key, do this only on this place for correct usage of the flag
   if(deleteSource && !followSymlink)
      this->setThreadKeyNftwFlushDiscard(true);

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
            this->logger->logErr("copyDir", "Could not delete directory after "
               "copy: " + std::string(source) + " Errno: " + System::getErrString(errno) );
      }
   }

   if(deleteSource)
      this->setThreadKeyNftwFlushDiscard(false);

   if(followSymlink)
   {
      this->setThreadKeyNftwFollowSymlink(false);
      this->resetThreadKeyNftwNumFollowedSymlinks();
   }

   return retVal;
}

/**
 * handles the followSymlink flag
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 * @param statSource the stat of the symlink
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache BeeGFS,
 *        false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the file/directory should be deleted after the file/directory was
 *        copied
 * @param ignoreDir true if directories should be ignored, should be set if the flag
 *        DEEPER_..._SUBDIRS wasn't used
 * @param doCRC true if a CRC checksum should be calculated
 * @param crcOutValue out value for the checksum if a checksum should be calculated
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::handleFollowSymlink(std::string sourcePath, std::string destPath,
   const struct stat* statSourceSymlink, bool copyToCache, bool deleteSource, bool ignoreDir,
   bool doCRC, unsigned long* crcOutValue)
{
   int retVal = DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   this->logger->logErr(__FUNCTION__, "source path: " + sourcePath + " - dest path: " + destPath);
#endif

   // get the new source path from the symlink value
   std::string pathFromLink;
   if(!CachePathTk::readLink(sourcePath.c_str(), statSourceSymlink, pathFromLink, this->logger) )
      return DEEPER_RETVAL_ERROR;

   // check relative paths
   if(pathFromLink[0] != '/')
   {
      std::string parent(dirname(strdup(sourcePath.c_str() ) ) );
      if(!CachePathTk::makePathAbsolute(pathFromLink, parent, this->logger) )
         return DEEPER_RETVAL_ERROR;
   }

   // check if the link doesn't point outside of the cache FS or the global FS
   if(copyToCache)
   {
      if(!CachePathTk::isGlobalPath(pathFromLink, cfg) )
      {
         this->logger->logErr(__FUNCTION__, "Link points outside of global FS: " + pathFromLink);

         return DEEPER_RETVAL_SUCCESS; // Do not report as an error, only log it to console
      }
   }
   else
   {
      if(!CachePathTk::isCachePath(pathFromLink, cfg) )
      {
         this->logger->logErr(__FUNCTION__, "Link points outside of cache FS: " + pathFromLink);

         return DEEPER_RETVAL_SUCCESS; // Do not report as an error, only log it to console
      }
   }

   struct stat statNewSourcePath;
   retVal = lstat(pathFromLink.c_str(), &statNewSourcePath);

   if(retVal == DEEPER_RETVAL_ERROR)
   {
      this->logger->logErr(__FUNCTION__, "Could not stat source file: " + pathFromLink +
         " Errno: " + System::getErrString(errno) );

      return DEEPER_RETVAL_ERROR;
   }

   // save the number of followed symlinks
   int backupNumFollowedSymlinks;
   backupNumFollowedSymlinks = this->getThreadKeyNftwNumFollowedSymlinks();

   // backup the paths from the previous follow symlink call for the restore after this call
   bool linkPathsAreSet = false;
   std::string backupSourceLinkPath;
   std::string backupDestLinkPath;
   linkPathsAreSet = this->getThreadKeyNftwPaths(backupSourceLinkPath, backupDestLinkPath);
   this->setThreadKeyNftwPaths(pathFromLink.c_str(), destPath.c_str() );

   // check if the maximum number of symlinks to follow is reached
   if(!this->testAndIncrementThreadKeyNftwNumFollowedSymlinks() )
   {
      errno = ELOOP;
      this->logger->logErr(__FUNCTION__, "Too many symbolic links were encountered: " +
         std::string(sourcePath) + " Errno: " + System::getErrString(errno) );

      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   if(S_ISDIR(statNewSourcePath.st_mode) )
   {
      if(unlikely(ignoreDir || doCRC) )
      {
         errno = EISDIR;
         this->logger->logErr(__FUNCTION__, "Symlink points to directory, but the flag "
            "DEEPER_..._FOLLOWSYMLINKS is set and the flag DEEPER_..._SUBDIRS is not set: " +
            pathFromLink + " Errno: " + System::getErrString(errno) );

         retVal = DEEPER_RETVAL_ERROR;
      }
      else
         retVal = copyDir(pathFromLink.c_str(), destPath.c_str(), copyToCache, deleteSource,
            true);
   }
   else
   if(S_ISREG(statNewSourcePath.st_mode) )
      retVal = copyFile(pathFromLink.c_str(), destPath.c_str(), &statNewSourcePath, deleteSource,
         doCRC, crcOutValue);
   else
   if(S_ISLNK(statNewSourcePath.st_mode) )
   {
      retVal = handleFollowSymlink(pathFromLink.c_str(), destPath, &statNewSourcePath, copyToCache,
         deleteSource, ignoreDir, doCRC, crcOutValue);
   }
   else
   {
      errno = EINVAL;
      this->logger->logErr(__FUNCTION__, "Unexpected typeflag value for path: " + pathFromLink);

      retVal = DEEPER_RETVAL_ERROR;
   }

   // reset the number of followed symlinks
   this->setThreadKeyNftwNumFollowedSymlinks(backupNumFollowedSymlinks);

   // restore the paths from the previous follow symlink call
   if(linkPathsAreSet)
      this->setThreadKeyNftwPaths(backupSourceLinkPath.c_str(), backupDestLinkPath.c_str() );
   else
      this->resetThreadKeyNftwPaths();

   return retVal;
}

/**
 * copies a file from global BeeGFS to the cache BeeGFS
 *
 * NOTE: This function is used by nftw , the interface of this function can not be changed.
 *
 * @param fpath the pathname of the entry relative to the parameter dirpath of the nftw(...) call
 * @param sb is a pointer to the stat structure returned by a call to stat for fpath
 * @param tflag typeflag of the entry which is given to this function by nftw, details in man page
 *        of nftw
 * @param ftwbuf controll flags for nftw, details in man page of nftw
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::nftwCopyFileToCache(const char *fpath, const struct stat *sb, int tflag,
   struct FTW *ftwbuf)
{
   int retVal = FTW_CONTINUE;

   DeeperCache* cache = DeeperCache::getInstance();

#ifdef BEEGFS_DEBUG
   cache->getLogger()->logErr("CopyFileToCache", "path: " + std::string(fpath) );
#endif

   std::string sourcePath(fpath);
   std::string destPath(fpath);

   bool deleteSource = cache->getThreadKeyNftwFlushDiscard();
   bool followSymlink = cache->getThreadKeyNftwFollowSymlink();
   bool isLinkPath = false;

   if(followSymlink)
   {
      std::string sourceLinkPath;
      std::string destLinkPath;

      if(cache->getThreadKeyNftwPaths(sourceLinkPath, destLinkPath) )
      {
         if(unlikely(!CachePathTk::replaceRootPath(destPath, sourceLinkPath, destLinkPath, true,
            cache->getLogger() ) ) )
         {
            errno = ENOENT;
            return FTW_STOP;
         }
         isLinkPath = true;
      }
   }

   if(!isLinkPath)
   {
      bool funcRetVal = CachePathTk::globalPathToCachePath(destPath, cache->getConfig(),
         cache->getLogger() );
      if(unlikely(!funcRetVal) )
      {
         errno = ENOENT;
         return FTW_STOP;
      }
   }

   retVal = cache->nftwHandleFlags(sourcePath, destPath, sb, tflag, true, deleteSource,
      followSymlink);

   return retVal;
}

/**
 * copies a file from cache BeeGFS to the global BeeGFS
 *
 * NOTE: This function is used by nftw , the interface of this function can not be changed.
 *
 * @param fpath the pathname of the entry relative to the parameter dirpath of the nftw(...) call
 * @param sb is a pointer to the stat structure returned by a call to stat for fpath
 * @param tflag typeflag of the entry which is given to this function by nftw, details in man page
 *        of nftw
 * @param ftwbuf controll flags for nftw, details in man page of nftw
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::nftwCopyFileToGlobal(const char *fpath, const struct stat *sb, int tflag,
   struct FTW *ftwbuf)
{
   int retVal = FTW_CONTINUE;

   DeeperCache* cache = DeeperCache::getInstance();

#ifdef BEEGFS_DEBUG
   cache->getLogger()->logErr("CopyFileToGlobal", "path: " + std::string(fpath) );
#endif

   std::string sourcePath(fpath);
   std::string destPath(fpath);

   bool deleteSource = cache->getThreadKeyNftwFlushDiscard();
   bool followSymlink = cache->getThreadKeyNftwFollowSymlink();
   bool isLinkPath = false;

   if(followSymlink)
   {
      std::string sourceLinkPath;
      std::string destLinkPath;

      if(cache->getThreadKeyNftwPaths(sourceLinkPath, destLinkPath) )
      {
         if(unlikely(!CachePathTk::replaceRootPath(destPath, sourceLinkPath, destLinkPath, true,
            cache->getLogger() ) ) )
         {
            errno = ENOENT;
            return FTW_STOP;
         }
         isLinkPath = true;
      }
   }

   if(!isLinkPath)
   {
      bool funcRetVal = CachePathTk::cachePathToGlobalPath(destPath, cache->getConfig(),
         cache->getLogger() );
      if(unlikely(!funcRetVal) )
      {
         errno = ENOENT;
         return FTW_STOP;
      }
   }

   retVal = cache->nftwHandleFlags(sourcePath, destPath, sb, tflag, false, deleteSource,
      followSymlink);

   return retVal;
}

/**
 * handles the nftw type flags
 *
 * NOTE: this function is called by the nftw functions
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 * @param sb is a pointer to the stat structure returned by a call to stat for the entry which was
 *        given to to a function by nftw, details in man page of nftw
 * @param tflag typeflag of the entry which was given to a function by nftw, details in man page of
 *        nftw
 * @param copyToCache true if the data should be copied from the global BeeGFS to the cache BeeGFS,
 *        false if the data should be copied from the cache BeeGFS to the global BeeGFS
 * @param deleteSource true if the file/directory should be deleted after the file/directory was
 *        copied
 * @param followSymlink true if the destination file or directory shoulb be copied and do not create
 *        symbolic links when a symbolic link was found
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::nftwHandleFlags(std::string sourcePath, std::string destPath,
   const struct stat *sb, int tflag, bool copyToCache, bool deleteSource, bool followSymlink)
{
   int retValInternal = FTW_CONTINUE;

   if(tflag == FTW_F)
   {
#ifdef BEEGFS_DEBUG
      this->logger->logErr("handleFlags", "source path: " + sourcePath + " - type: file");
#endif

      retValInternal = this->copyFile(sourcePath.c_str(), destPath.c_str(), sb, deleteSource,
         false, NULL);
      if(retValInternal == DEEPER_RETVAL_ERROR)
      {
         this->logger->logErr("handleFlags", "Could not copy file from " +
            sourcePath + " to " + destPath + " Errno: " + System::getErrString(errno) );

         return FTW_STOP;
      }
   }
   else
   if( (tflag == FTW_D) || (tflag == FTW_DP) )
   {
#ifdef BEEGFS_DEBUG
      this->logger->logErr("handleFlags", "source path: " + sourcePath + " - type: directory");
#endif

      retValInternal = mkdir(destPath.c_str(), sb->st_mode);
      if( (retValInternal == DEEPER_RETVAL_ERROR) && (errno != EEXIST) )
      {
         this->logger->logErr("handleFlags", "Could not create directory " +
            destPath + " Errno: " + System::getErrString(errno) );

         return FTW_STOP;
      }

      retValInternal = chown(destPath.c_str(), sb->st_uid, sb->st_gid);
      if( (retValInternal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
      { // EPERM happens when the process is not running as root, this can be ignored
         this->logger->logErr("handleFlags", "Could not change owner of directory " +
            destPath + " Errno: " + System::getErrString(errno) );

         return FTW_STOP;
      }

      /**
       * the flag deleteSource need to be handled in a second nftw tree walk, because at this point
       * the directories are not empty
       */
   }
   else
   if(tflag == FTW_SL)
   {
#ifdef BEEGFS_DEBUG
      this->logger->logErr("handleFlags", "source path: " + sourcePath + " - type: link");
#endif
      if(followSymlink)
         retValInternal = handleFollowSymlink(sourcePath, destPath, sb, copyToCache, deleteSource,
            false, false, NULL);
      else
         retValInternal = createSymlink(sourcePath.c_str(), destPath.c_str(), sb, copyToCache,
            deleteSource);

      if(retValInternal == DEEPER_RETVAL_ERROR)
         return FTW_STOP;
   }
   else
   if( (tflag == FTW_DNR) || (tflag == FTW_NS) || (tflag == FTW_SLN) )
   {
      errno = EINVAL;
      this->logger->logErr("handleFlags", "unexpected typeflag value for path: " + sourcePath);
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
 * @param tflag typeflag of the entry which is given to this function by nftw, details in man page
 *        of nftw
 * @param ftwbuf controll flags for nftw, details in man page of nftw
 * @return 0 on success, -1 and errno set in case of error.
 */
int DeeperCache::nftwDelete(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
   DeeperCache* cache = DeeperCache::getInstance();
   bool followSymlink = cache->getThreadKeyNftwFollowSymlink();

   if( (tflag == FTW_D) || (tflag == FTW_DP) )
   {
#ifdef BEEGFS_DEBUG
      DeeperCache::getInstance()->getLogger()->logErr("delete", "source path: " +
         std::string(fpath) + " - type: directory");
#endif
      if(rmdir(fpath) )
      {
         DeeperCache::getInstance()->getLogger()->logErr("delete", "Could not delete directory "
            "after copy " + std::string(fpath) + " Errno: " + System::getErrString(errno) );

         return FTW_STOP;
      }
   }
   else
   if( (tflag == FTW_F) || (tflag == FTW_SL) )
   {
#ifdef BEEGFS_DEBUG
      DeeperCache::getInstance()->getLogger()->logErr("delete", "source path: " +
         std::string(fpath) + " - type: file or symlink");
#endif
      if(unlikely(!followSymlink) )
      {
         DeeperCache::getInstance()->getLogger()->logErr("delete",
            "found a file or symlink: " + std::string(fpath) + " all files and symlinks should be "
            "deleted");

         return FTW_STOP;
      }

      if(unlink(fpath) )
      {
         DeeperCache::getInstance()->getLogger()->logErr("delete", "Could not delete file or "
            "symlink: " + std::string(fpath) );

         return FTW_STOP;
      }
   }
   else
   if( (tflag == FTW_DNR) || (tflag == FTW_NS) || (tflag == FTW_SLN) )
   {
      DeeperCache::getInstance()->getLogger()->logErr("delete", "unexpected typeflag value");
      return FTW_STOP;
   }

   return FTW_CONTINUE;
}

/**
 * constructor for the thread variable of the flush flag from the last nftw(...) call
 */
void DeeperCache::makeThreadKeyNftwFlushDiscard()
{
   pthread_key_create(&nftwFlushDiscard, freeThreadKeyNftwFlushDiscard);
}

/**
 * destructor of the thread variable of the flush flag from the last nftw(...) call
 *
 * @param value pointer of the thread variable
 */
void DeeperCache::freeThreadKeyNftwFlushDiscard(void* value)
{
   SAFE_DELETE_NOSET((bool*) value);
}

/**
 * stores the flush flags which was given to nftw(...) as a thread variable
 *
 * @param flushFlag the path to save as thread variable
 */
void DeeperCache::setThreadKeyNftwFlushDiscard(bool flushFlag)
{
   pthread_once(&nftwKeyOnceFlushDiscard, makeThreadKeyNftwFlushDiscard);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwFlushDiscard)) != NULL)
      SAFE_DELETE_NOSET((bool*) ptr);

   ptr = new bool(flushFlag);
   pthread_setspecific(nftwFlushDiscard, ptr);
}

/**
 * reads the thread variable which contains the flush flags of the last nftw(...) call
 *
 * @return flush flags of the last nftw(...) call
 */
bool DeeperCache::getThreadKeyNftwFlushDiscard()
{
   pthread_once(&nftwKeyOnceFlushDiscard, makeThreadKeyNftwFlushDiscard);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwFlushDiscard)) != NULL)
      return bool(*(bool*) ptr);

   return false; // not the best solution, but doesn't destroy the data
}

/**
 * constructor for the thread variable of the followSymlink flag from the last nftw(...) call
 */
void DeeperCache::makeThreadKeyNftwFollowSymlink()
{
   pthread_key_create(&nftwFollowSymlink, freeThreadKeyNftwFollowSymlink);
}

/**
 * destructor of the thread variable of the followSymlink flag from the last nftw(...) call
 *
 * @param value pointer of the thread variable
 */
void DeeperCache::freeThreadKeyNftwFollowSymlink(void* value)
{
   SAFE_DELETE_NOSET((bool*) value);
}

/**
 * stores the followSymlink flags which was given to nftw(...) as a thread variable
 *
 * @param followSymlink the path to save as thread variable
 */
void DeeperCache::setThreadKeyNftwFollowSymlink(bool followSymlinkFlag)
{
   pthread_once(&nftwKeyOnceFollowSymlink, makeThreadKeyNftwFollowSymlink);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwFollowSymlink)) != NULL)
      SAFE_DELETE_NOSET((bool*) ptr);

   ptr = new bool(followSymlinkFlag);
   pthread_setspecific(nftwFollowSymlink, ptr);
}

/**
 * reads the thread variable which contains the followSymlink flags of the last nftw(...) call
 *
 * @return followSymlink flags of the last nftw(...) call
 */
bool DeeperCache::getThreadKeyNftwFollowSymlink()
{
   pthread_once(&nftwKeyOnceFollowSymlink, makeThreadKeyNftwFollowSymlink);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwFollowSymlink)) != NULL)
      return bool(*(bool*) ptr);

   return false; // not the best solution, but doesn't destroy the data
}

/**
 * constructor for the thread variable of the numFollowedSymlinks counter from the last nftw(...)
 * call
 */
void DeeperCache::makeThreadKeyNftwNumFollowedSymlinks()
{
   pthread_key_create(&nftwNumFollowedSymlinks, freeThreadKeyNftwNumFollowedSymlinks);
}

/**
 * destructor of the thread variable of the numFollowedSymlinks counter from the last nftw(...) call
 *
 * @param value pointer of the thread variable
 */
void DeeperCache::freeThreadKeyNftwNumFollowedSymlinks(void* value)
{
   SAFE_DELETE_NOSET((int*) value);
}

/**
 * stores the numFollowedSymlinks counter which is given to nftw(...) as a thread variable
 *
 * @param followSymlink the path to save as thread variable
 */
void DeeperCache::setThreadKeyNftwNumFollowedSymlinks(int numFollowedSymlinks)
{
   pthread_once(&nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwNumFollowedSymlinks)) != NULL)
      SAFE_DELETE_NOSET((int*) ptr);

   ptr = new int(numFollowedSymlinks);
   pthread_setspecific(nftwNumFollowedSymlinks, ptr);

#ifdef BEEGFS_DEBUG
      char buffer[20];
      char bufferMax[20];
      sprintf(buffer, "%d", numFollowedSymlinks );
      sprintf(bufferMax, "%d", MAX_NUM_FOLLOWED_SYMLINKS );
      this->logger->logErr(__FUNCTION__, std::string("number of followed symlinks: ") + buffer +
         std::string(" ;max allowed symlinks to follow ") + bufferMax);
#endif
}

/**
 * reads the thread variable which contains the numFollowedSymlinks counter of the last nftw(...)
 * call
 *
 * @return the number of symlinks was followed
 */
int DeeperCache::getThreadKeyNftwNumFollowedSymlinks()
{
   pthread_once(&nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwNumFollowedSymlinks)) != NULL)
   {
#ifdef BEEGFS_DEBUG
      char buffer[20];
      char bufferMax[20];
      sprintf(buffer, "%d", int(*(int*) ptr) );
      sprintf(bufferMax, "%d", MAX_NUM_FOLLOWED_SYMLINKS );
      this->logger->logErr(__FUNCTION__, std::string("number of followed symlinks: ") + buffer +
         std::string(" ;max allowed symlinks to follow ") + bufferMax);
#endif
      return int(*(int*) ptr);
   }

#ifdef BEEGFS_DEBUG
      char buffer[20];
      char bufferMax[20];
      sprintf(buffer, "%d", 0);
      sprintf(bufferMax, "%d", MAX_NUM_FOLLOWED_SYMLINKS );
      this->logger->logErr(__FUNCTION__, std::string("number of followed symlinks: ") + buffer +
         std::string(" ;max allowed symlinks to follow ") + bufferMax);
#endif

   return 0;
}

/**
 * resets the numFollowedSymlinks counter to zero, the counter was given to nftw(...) as a thread
 * variable, it deletes the thread variable
 */
void DeeperCache::resetThreadKeyNftwNumFollowedSymlinks()
{
   pthread_once(&nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwNumFollowedSymlinks)) != NULL)
   {
      SAFE_DELETE_NOSET((int*) ptr);

      ptr = new int(0);
      pthread_setspecific(nftwNumFollowedSymlinks, ptr);
   }

#ifdef BEEGFS_DEBUG
      char buffer[20];
      char bufferMax[20];
      sprintf(buffer, "%d", 0);
      sprintf(bufferMax, "%d", MAX_NUM_FOLLOWED_SYMLINKS );
      this->logger->logErr(__FUNCTION__, std::string("number of followed symlinks: ") + buffer +
         std::string(" ;max allowed symlinks to follow ") + bufferMax);
#endif
}

/**
 * tests if the numFollowedSymlinks counter has reached the maximum value, the counter was given to
 * nftw(...) as a thread variable
 */
bool DeeperCache::testAndIncrementThreadKeyNftwNumFollowedSymlinks()
{
   pthread_once(&nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

   void* ptr;
   int counter = 0;

   if ( (ptr = pthread_getspecific(nftwNumFollowedSymlinks) ) != NULL)
   {
      counter = *(int*)ptr;

      if(counter < MAX_NUM_FOLLOWED_SYMLINKS)
         SAFE_DELETE_NOSET((int*) ptr);
   }

   counter++;

   if(counter <= MAX_NUM_FOLLOWED_SYMLINKS)
   {
      ptr = new int(counter);
      pthread_setspecific(nftwNumFollowedSymlinks, ptr);

#ifdef BEEGFS_DEBUG
      char buffer[20];
      char bufferMax[20];
      sprintf(buffer, "%d", counter );
      sprintf(bufferMax, "%d", MAX_NUM_FOLLOWED_SYMLINKS );
      this->logger->logErr(__FUNCTION__, std::string("number of followed symlinks: ") + buffer +
         std::string(" ;max allowed symlinks to follow ") + bufferMax);
#endif

      return true;
   }

#ifdef BEEGFS_DEBUG
      char buffer[20];
      char bufferMax[20];
      sprintf(buffer, "%d", counter );
      sprintf(bufferMax, "%d", MAX_NUM_FOLLOWED_SYMLINKS );
      this->logger->logErr(__FUNCTION__, std::string("number of followed symlinks: ") + buffer +
         std::string(" ;max allowed symlinks to follow ") + bufferMax);
#endif

   return false;
}

/**
 * constructor for the thread variable of the flush flag from the last nftw(...) call
 */
void DeeperCache::makeThreadKeyNftwDestPath()
{
   pthread_key_create(&nftwDestPath, freeThreadKeyNftwDestPath);
}

/**
 * destructor of the thread variable of the flush flag from the last nftw(...) call
 *
 * @param value pointer of the thread variable
 */
void DeeperCache::freeThreadKeyNftwDestPath(void* value)
{
   SAFE_DELETE_NOSET((std::string*) value);
}

/**
 * stores the flush flags which was given to nftw(...) as a thread variable
 *
 * @param destPath the path to save as thread variable
 */
void DeeperCache::setThreadKeyNftwDestPath(const char* destPath)
{
   pthread_once(&nftwKeyOnceDestPath, makeThreadKeyNftwDestPath);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwDestPath)) != NULL)
      SAFE_DELETE_NOSET((std::string*) ptr);

#ifdef BEEGFS_DEBUG
      this->logger->logErr(__FUNCTION__, "new destPath: " + std::string(destPath) );
#endif

   ptr = new std::string(destPath);
   pthread_setspecific(nftwDestPath, ptr);
}

/**
 * reads the thread variable which contains the flush flags of the last nftw(...) call
 *
 * @return flush flags of the last nftw(...) call
 */
bool DeeperCache::getThreadKeyNftwDestPath(std::string& outValue)
{
   pthread_once(&nftwKeyOnceDestPath, makeThreadKeyNftwDestPath);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwDestPath) ) != NULL)
   {
      if(!(*(std::string*) ptr).empty() )
      {
         outValue.assign(*(std::string*) ptr);
         return true;
      }
   }

   return false;
}

/**
 * resets the numFollowedSymlinks counter to zero, the counter was given to nftw(...) as a thread
 * variable, it deletes the thread variable
 */
void DeeperCache::resetThreadKeyNftwDestPath()
{
   pthread_once(&nftwKeyOnceDestPath, makeThreadKeyNftwDestPath);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwDestPath)) != NULL)
   {
      SAFE_DELETE_NOSET((int*) ptr);

      ptr = new std::string("");
      pthread_setspecific(nftwDestPath, ptr);
   }
}

/**
 * constructor for the thread variable of the flush flag from the last nftw(...) call
 */
void DeeperCache::makeThreadKeyNftwSourcePath()
{
   pthread_key_create(&nftwSourcePath, freeThreadKeyNftwSourcePath);
}

/**
 * destructor of the thread variable of the flush flag from the last nftw(...) call
 *
 * @param value pointer of the thread variable
 */
void DeeperCache::freeThreadKeyNftwSourcePath(void* value)
{
   SAFE_DELETE_NOSET((std::string*) value);
}

/**
 * stores the flush flags which was given to nftw(...) as a thread variable
 *
 * @param destPath the path to save as thread variable
 */
void DeeperCache::setThreadKeyNftwSourcePath(const char* sourcePath)
{
   pthread_once(&nftwKeyOnceSourcePath, makeThreadKeyNftwSourcePath);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwSourcePath)) != NULL)
      SAFE_DELETE_NOSET((std::string*) ptr);

#ifdef BEEGFS_DEBUG
      this->logger->logErr(__FUNCTION__, "new destPath: " + std::string(sourcePath) );
#endif

   ptr = new std::string(sourcePath);
   pthread_setspecific(nftwSourcePath, ptr);
}

/**
 * reads the thread variable which contains the flush flags of the last nftw(...) call
 *
 * @return flush flags of the last nftw(...) call
 */
bool DeeperCache::getThreadKeyNftwSourcePath(std::string& outValue)
{
   pthread_once(&nftwKeyOnceSourcePath, makeThreadKeyNftwSourcePath);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwSourcePath) ) != NULL)
   {
      if(!(*(std::string*) ptr).empty() )
      {
         outValue.assign(*(std::string*) ptr);
         return true;
      }
   }

   return false;
}

/**
 * resets the numFollowedSymlinks counter to zero, the counter was given to nftw(...) as a thread
 * variable, it deletes the thread variable
 */
void DeeperCache::resetThreadKeyNftwSourcePath()
{
   pthread_once(&nftwKeyOnceSourcePath, makeThreadKeyNftwSourcePath);

   void* ptr;

   if ((ptr = pthread_getspecific(nftwSourcePath)) != NULL)
   {
      SAFE_DELETE_NOSET((int*) ptr);

      ptr = new std::string("");
      pthread_setspecific(nftwSourcePath, ptr);
   }
}

/**
 * stores the flush flags which was given to nftw(...) as a thread variable
 *
 * @param destPath the path to save as thread variable
 */
void DeeperCache::setThreadKeyNftwPaths(const char* sourcePath, const char* destPath)
{
   this->setThreadKeyNftwSourcePath(sourcePath);
   this->setThreadKeyNftwDestPath(destPath);
}

/**
 * stores the flush flags which was given to nftw(...) as a thread variable
 *
 * @param destPath the path to save as thread variable
 */
bool DeeperCache::getThreadKeyNftwPaths(std::string& sourcePath, std::string& destPath)
{
    return (this->getThreadKeyNftwSourcePath(sourcePath) &&
       this->getThreadKeyNftwDestPath(destPath) );
}

/**
 * stores the flush flags which was given to nftw(...) as a thread variable
 *
 * @param destPath the path to save as thread variable
 */
void DeeperCache::resetThreadKeyNftwPaths()
{
   this->resetThreadKeyNftwSourcePath();
   this->resetThreadKeyNftwDestPath();
}
