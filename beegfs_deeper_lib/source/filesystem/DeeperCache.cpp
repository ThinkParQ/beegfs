#include <common/cache/net/message/flush/FlushCrcMsg.h>
#include <common/cache/net/message/flush/FlushCrcRespMsg.h>
#include <common/cache/net/message/flush/FlushIsFinishedMsg.h>
#include <common/cache/net/message/flush/FlushIsFinishedRespMsg.h>
#include <common/cache/net/message/flush/FlushRangeMsg.h>
#include <common/cache/net/message/flush/FlushRangeRespMsg.h>
#include <common/cache/net/message/flush/FlushMsg.h>
#include <common/cache/net/message/flush/FlushRespMsg.h>
#include <common/cache/net/message/flush/FlushStopMsg.h>
#include <common/cache/net/message/flush/FlushStopRespMsg.h>
#include <common/cache/net/message/flush/FlushWaitMsg.h>
#include <common/cache/net/message/flush/FlushWaitRespMsg.h>
#include <common/cache/net/message/flush/FlushWaitCrcMsg.h>
#include <common/cache/net/message/flush/FlushWaitCrcRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchCrcMsg.h>
#include <common/cache/net/message/prefetch/PrefetchCrcRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchIsFinishedMsg.h>
#include <common/cache/net/message/prefetch/PrefetchIsFinishedRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchRangeMsg.h>
#include <common/cache/net/message/prefetch/PrefetchRangeRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchMsg.h>
#include <common/cache/net/message/prefetch/PrefetchRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchStopMsg.h>
#include <common/cache/net/message/prefetch/PrefetchStopRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchWaitCrcMsg.h>
#include <common/cache/net/message/prefetch/PrefetchWaitCrcRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchWaitMsg.h>
#include <common/cache/net/message/prefetch/PrefetchWaitRespMsg.h>
#include <common/cache/toolkit/cachepathtk.h>
#include <common/cache/toolkit/filesystemtk.h>
#include <common/cache/toolkit/threadvartk.h>
#include <common/net/message/NetMessageTypes.h>
#include <common/nodes/NodeType.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/StringTk.h>
#include <deeper/deeper_cache.h>

#include "DeeperCache.h"


#include <libgen.h>
#include <sys/stat.h>
#include <zlib.h>


const int DEEPERCACHE_WAIT_SLEEP_SEC = 3;

// Static data member for singleton which is visible to external programs which use this lib.
DeeperCache* DeeperCache::cacheInstance = NULL;
App* DeeperCache::app = NULL;

/**
 * Constructor.
 */
DeeperCache::DeeperCache()
{
   char* argvLib[1];
   argvLib[0] = (char*) malloc(20);
   strcpy(argvLib[0], "beegfs_deeper_lib");
   int argcLib = 1;

   // use a app object to make the Configuration and the logger globally available by the default
   // mechanism
   AbstractApp::runTimeInitsAndChecks();
   app = new App(argcLib, argvLib);
   PThread::initFakeThread(app);

   std::string nodeID = System::getHostname();
   this->localNode = std::make_shared<LocalNode>(nodeID, NumNodeID(0), 0, 0,
      app->getConfig()->getConnNamedSocket(), NODETYPE_CacheDaemon); // use destination NodeType

   free(argvLib[0]);
}

/**
 * Destructor.
 */
DeeperCache::~DeeperCache()
{
   localNode.reset();
   SAFE_DELETE(this->app);
}

/**
 * Initialize or get a deeper-cache object as a singleton.
 */
DeeperCache* DeeperCache::getInstance()
{
   if(!cacheInstance)
   {
      cacheInstance = new DeeperCache();
   }
   else
   {
      // add the App to the thread environment, maybe it is a new thread which doesn't have a valid
      // which doesn't have a thread variable with the app object, it is required to use the
      // beegfs_common functions
      PThread::initFakeThread(app);
   }

   return cacheInstance;
}

/**
 * Ćleanup method to call the destructor in the lib.
 */
void DeeperCache::cleanup()
{
   SAFE_DELETE(cacheInstance);
}


/**
 * Create a new directory on the cache layer of the current cache domain.
 *
 * @param path Path and name of new directory.
 * @param mode Permission bits, similar to POSIX mkdir (S_IRWXU etc.).
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_mkdir(const char *path, mode_t mode)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string newPath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + newPath);
#endif

   if(cachepathtk::globalPathToCachePath(newPath, app->getConfig(), Logger::getLogger() ) )
      retVal = mkdir(newPath.c_str(), mode);
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Remove a directory from the cache layer.
 *
 * @param path Path and name of directory, which should be removed.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_rmdir(const char *path)
{
   int retVal = DEEPER_RETVAL_ERROR;
   std::string newPath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + newPath);
#endif

   if(cachepathtk::globalPathToCachePath(newPath, app->getConfig(), Logger::getLogger() ) )
      retVal = rmdir(newPath.c_str() );
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Open a directory stream for a directory on the cache layer of the current cache domain and
 * position the stream at the first directory entry.
 *
 * @param name Path and name of directory on cache layer.
 * @return Pointer to directory stream, NULL and errno set in case of error.
 */
DIR* DeeperCache::cache_opendir(const char *name)
{
   DIR* retVal = NULL;
   std::string newName(name);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + newName);
#endif

   if(cachepathtk::globalPathToCachePath(newName, app->getConfig(), Logger::getLogger() ) )
      retVal = opendir(newName.c_str() );
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Close directory stream.
 *
 * @param dirp Directory stream, which should be closed.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
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
 * @param path Path and name of file, which should be opened.
 * @param oflag Access mode flags, similar to POSIX open (O_WRONLY, O_CREAT, etc).
 * @param mode The permissions of the file, similar to POSIX open. When oflag contains a file
 *        creation flag (O_CREAT, O_EXCL, O_NOCTTY, and O_TRUNC) the mode flag is required in other
 *        cases this flag is ignored.
 * @param deeper_open_flags DEEPER_OPEN_NONE or a combination of the following flags:
 *        DEEPER_OPEN_FLUSHONCLOSE to automatically flush written data to global
 *           storage when the file is closed, asynchronously.
 *        DEEPER_OPEN_FLUSHWAIT to make DEEPER_OPEN_FLUSHONCLOSE a synchronous operation, which
 *        means the close operation will only return after flushing is complete.
 *        DEEPER_OPEN_DISCARD to remove the file from the cache layer after it has been closed.
 * @return File descriptor as non-negative integer on success, DEEPER_RETVAL_ERROR and errno set
 *         in case of error.
 */
int DeeperCache::cache_open(const char* path, int oflag, mode_t mode, int deeper_open_flags)
{
   int retFD = DEEPER_RETVAL_ERROR;
   std::string newName(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + newName);
#endif

   if(!cachepathtk::globalPathToCachePath(newName, app->getConfig(), Logger::getLogger() ) )
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
 * @param fildes File descriptor of open file.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_close(int fildes)
{
   int retVal = DEEPER_RETVAL_ERROR;
   bool doFlush = false;


   DeeperCacheSession session(fildes);
   getAndRemoveEntryFromSessionMap(session);

   // check the session flags
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

   // the asynchronous stuff is done in the cache_flush(...) function
   if(doFlush)
      retVal = cache_flush(session.getPath().c_str(), deeperFlushFlags);

   // delete the file if a flush is not requested
   if( (!doFlush) && ( (session.getDeeperOpenFlags() & DEEPER_OPEN_DISCARD) != 0) )
   {
      std::string cachePath(session.getPath() );
      if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
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
 * @param path Path to file or directory, which should be prefetched.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or a combination of the following flags:
 *        DEEPER_PREFETCH_SUBDIRS to recursively copy all subdirs, if given path leads to a
 *           directory.
 *        DEEPER_PREFETCH_WAIT To make this a synchronous operation.
 *        DEEPER_PREFETCH_FOLLOWSYMLINKS To copy the destination file or directory and do not
 *           create symbolic links when a symbolic link was found.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_prefetch(const char* path, int deeper_prefetch_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + cachePath);
#endif

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }


   if(!(deeper_prefetch_flags & DEEPER_PREFETCH_WAIT) )
   { // the asynchronous implementation
      int cacheErrorCode = DEEPER_RETVAL_ERROR;

      bool commRes;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      PrefetchRespMsg* respMsgCast;

      PrefetchMsg prefetchMsg(std::string(path), cachePath, deeper_prefetch_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &prefetchMsg,
         NETMSGTYPE_CachePrefetchResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (PrefetchRespMsg*)respMsg;

      cacheErrorCode = respMsgCast->getValue();
      if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Couldn't submit asynchronous prefetch on path: " + std::string(path) +
            "; errno: " + System::getErrString(cacheErrorCode) );
         errno = cacheErrorCode;
      }
      else
      {
         retVal = DEEPER_RETVAL_SUCCESS;
      }


err_cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);
   }
   else if(deeper_prefetch_flags & DEEPER_PREFETCH_SUBDIRS)
   { // the synchronous implementation for directory traversal
      retVal = filesystemtk::handleSubdirectoryFlag(path, cachePath.c_str(), true, false,
         deeper_prefetch_flags & DEEPER_PREFETCH_FOLLOWSYMLINKS);
   }
   else
   { // the synchronous implementation
      if(!cachepathtk::createPath(cachePath, app->getConfig()->getSysMountPointGlobal(),
         app->getConfig()->getSysMountPointCache(), true, Logger::getLogger() ) )
            return DEEPER_RETVAL_ERROR;

      struct stat statSource;
      int funcError = lstat(path, &statSource);
      if(funcError != 0)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            std::string("Could not stat source file/directory ") + path +
            "; errno: " + System::getErrString(errno) );
         return DEEPER_RETVAL_ERROR;
      }

      if(S_ISLNK(statSource.st_mode) )
      {
         if(deeper_prefetch_flags & DEEPER_PREFETCH_FOLLOWSYMLINKS)
         {
            threadvartk::setThreadKeyNftwFollowSymlink(true);

            retVal = filesystemtk::handleFollowSymlink(path, cachePath.c_str(),  &statSource, true,
               false, true, false, NULL);

            //reset thread variables
            threadvartk::setThreadKeyNftwFollowSymlink(false);
            threadvartk::resetThreadKeyNftwNumFollowedSymlinks();
         }
         else
         {
            retVal = filesystemtk::createSymlink(path, cachePath.c_str(), &statSource, true, false);
         }
      }
      else if(S_ISREG(statSource.st_mode) )
      {
         retVal = filesystemtk::copyFile(path, cachePath.c_str(), &statSource, false, false, NULL,
            NULL);
      }
      else if(S_ISDIR(statSource.st_mode) )
      {
         errno = EISDIR;
         Logger::getLogger()->logErr(__FUNCTION__, std::string(
            "Given path is an directory but DEEPER_PREFETCH_SUBDIRS flag is not set: ") + path);
         retVal = DEEPER_RETVAL_ERROR;
      }
      else
      {
         errno = EINVAL;
         Logger::getLogger()->logErr(__FUNCTION__,
            std::string("Unsupported file type for path: ") + path);
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
 * @param path Path to file or directory, which should be prefetched.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or a combination of the following flags:
 *        DEEPER_PREFETCH_WAIT To make this a synchronous operation.
 *        DEEPER_PREFETCH_FOLLOWSYMLINKS To copy the destination file or directory and do not
 *           create symbolic links when a symbolic link was found.
 * @param outChecksum The checksum of the file, it is only used in the synchronous case.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_prefetch_crc(const char* path, int deeper_prefetch_flags,
   unsigned long* outChecksum)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + cachePath);
#endif

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(!cachepathtk::createPath(cachePath, app->getConfig()->getSysMountPointGlobal(),
      app->getConfig()->getSysMountPointCache(), true, Logger::getLogger() ) )
         return DEEPER_RETVAL_ERROR;


   if(!(deeper_prefetch_flags & DEEPER_PREFETCH_WAIT) )
   { // the asynchronous implementation
      int cacheErrorCode = DEEPER_RETVAL_ERROR;

      bool commRes;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      PrefetchCrcRespMsg* respMsgCast;

      PrefetchCrcMsg prefetchCrcMsg(std::string(path), cachePath, deeper_prefetch_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &prefetchCrcMsg,
         NETMSGTYPE_CachePrefetchCrcResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (PrefetchCrcRespMsg*)respMsg;

      cacheErrorCode = respMsgCast->getValue();
      if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Couldn't submit asynchronous prefetch with CRC checksum calculation on path: " +
            std::string(path) + "; errno: " + System::getErrString(cacheErrorCode) );
         errno = cacheErrorCode;
      }
      else
      {
         retVal = DEEPER_RETVAL_SUCCESS;
      }


err_cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);

      return retVal;
   }


   // the synchronous implementation
   struct stat statSource;
   int funcError = lstat(path, &statSource);
   if(funcError != 0)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         std::string("Could not stat source file/directory: ") + path +
         "; errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   if(S_ISREG(statSource.st_mode) )
   {
      retVal = filesystemtk::copyFile(path, cachePath.c_str(), &statSource, false, true,
         NULL, outChecksum);
   }
   else if(S_ISLNK(statSource.st_mode) )
   {
      if(deeper_prefetch_flags & DEEPER_PREFETCH_FOLLOWSYMLINKS)
      {
         threadvartk::setThreadKeyNftwFollowSymlink(true);

         retVal = filesystemtk::handleFollowSymlink(path, cachePath.c_str(),  &statSource, true,
            false, true, true, outChecksum);

         //reset thread variables
         threadvartk::setThreadKeyNftwFollowSymlink(false);
         threadvartk::resetThreadKeyNftwNumFollowedSymlinks();
      }
      else
      {
         errno = ENOLINK;
         Logger::getLogger()->logErr(__FUNCTION__, std::string("Given path is a symlink."
            "CRC checksum calculation requires a file or the DEEPER_PREFETCH_FOLLOWSYMLINKS flag: ")
            + path);
         retVal = DEEPER_RETVAL_ERROR;
      }
   }
   else if(S_ISDIR(statSource.st_mode) )
   {
      errno = EISDIR;
      Logger::getLogger()->logErr(__FUNCTION__, std::string("Given path is an directory."
         "CRC checksum calculation doesn't support directories: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   {
      errno = EINVAL;
      Logger::getLogger()->logErr(__FUNCTION__,
         std::string("Unsupported file type for path: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }

   return retVal;
}

/**
 * Prefetch a file similar to deeper_cache_prefetch(), but prefetch only a certain range, not the
 * whole file.
 *
 * @param path Path to file, which should be prefetched.
 * @param pos Start position (offset) of the byte range that should be flushed.
 * @param num_bytes Number of bytes from pos that should be flushed.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or the following flag:
  *        DEEPER_PREFETCH_WAIT to make this a synchronous operation.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_prefetch_range(const char* path, off_t start_pos, size_t num_bytes,
   int deeper_prefetch_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + cachePath);
#endif

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }


   if(!(deeper_prefetch_flags & DEEPER_PREFETCH_WAIT) )
   { // the asynchronous implementation
      int cacheErrorCode = DEEPER_RETVAL_ERROR;

      bool commRes;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      PrefetchRangeRespMsg* respMsgCast;

      PrefetchRangeMsg prefetchRangeMsg(std::string(path), cachePath, deeper_prefetch_flags,
         start_pos, num_bytes);

      commRes = MessagingTk::requestResponse(getLocalNode(), &prefetchRangeMsg,
         NETMSGTYPE_CachePrefetchRangeResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (PrefetchRangeRespMsg*)respMsg;

      cacheErrorCode = respMsgCast->getValue();
      if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Couldn't submit asynchronous range prefetch on path: " + std::string(path) +
            "; errno: " + System::getErrString(cacheErrorCode) );
         errno = cacheErrorCode;
      }
      else
      {
         retVal = DEEPER_RETVAL_SUCCESS;
      }


err_cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);

      return retVal;
   }


   // the synchronous implementation
   if(!cachepathtk::createPath(cachePath, app->getConfig()->getSysMountPointGlobal(),
      app->getConfig()->getSysMountPointCache(), true, Logger::getLogger() ) )
         return DEEPER_RETVAL_ERROR;

   retVal = filesystemtk::copyFileRange(path, cachePath.c_str(), NULL, &start_pos, num_bytes, NULL,
      true);

   return retVal;
}

/**
 * Wait for an ongoing prefetch operation from deeper_cache_prefetch[_range]() to complete.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or a combination of the following flags:
 *        DEEPER_PREFETCH_SUBDIRS To recursively wait contents of all subdirs, if given path leads
 *           to a directory.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_prefetch_wait(const char* path, int deeper_prefetch_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;
   int cacheErrorCode = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   PrefetchWaitRespMsg* respMsgCast;

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   // check if the given path exists in the cache FS or in the global FS, if not there couldn't be
   // a prefetch operation
   struct stat statSource;
   int funcError = lstat(path, &statSource);
   if(funcError != 0)
   {
      struct stat statDest;
      funcError = lstat(cachePath.c_str(), &statDest);
      if(funcError != 0)
      {
         Logger::getLogger()->logErr(__FUNCTION__, "Could not stat source file/directory " +
            std::string(path) + " and the destination file/directory " + cachePath +
            ". No ongoing prefetch for the given path possible. "
            "errno: " + System::getErrString(errno) );
         errno = EINVAL;
         return DEEPER_RETVAL_ERROR;
      }
   }

   do
   {
      PrefetchWaitMsg waitMsg(std::string(path), cachePath, deeper_prefetch_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &waitMsg,
         NETMSGTYPE_CachePrefetchWaitResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (PrefetchWaitRespMsg*)respMsg;
      cacheErrorCode = respMsgCast->getValue();

      if(cacheErrorCode == EBUSY)
      {
         SAFE_DELETE(respMsg);
         SAFE_FREE(respBuf);

         // wait a few seconds before trying again
         ::sleep(DEEPERCACHE_WAIT_SLEEP_SEC);
      }
   } while (cacheErrorCode == EBUSY);

   if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Wait for asynchronous prefetch failed on path: " + std::string(path) +
         "; errno: " + System::getErrString(cacheErrorCode) );
      errno = cacheErrorCode;
   }
   else
   {
      retVal = DEEPER_RETVAL_SUCCESS;
   }


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Wait for an ongoing prefetch operation from deeper_cache_prefetch_crc() to complete.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE. Currently ignored, but maybe later some flags
 *           required.
 * @param outChecksum The checksum of the file.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_prefetch_wait_crc(const char* path, int deeper_prefetch_flags,
   unsigned long* outChecksum)
{
   int retVal = DEEPER_RETVAL_ERROR;
   int cacheErrorCode = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   PrefetchWaitCrcRespMsg* respMsgCast;

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   // check if the given path exists in the cache FS or in the global FS, if not there couldn't be
   // a prefetch operation
   struct stat statSource;
   int funcError = lstat(path, &statSource);
   if(funcError != 0)
   {
      struct stat statDest;
      funcError = lstat(cachePath.c_str(), &statDest);
      if(funcError != 0)
      {
         Logger::getLogger()->logErr(__FUNCTION__, "Could not stat source file/directory " +
            std::string(path) + " and the destination file/directory " + cachePath +
            ". No ongoing prefetch for the given path possible. "
            "errno: " + System::getErrString(errno) );
         errno = EINVAL;
         return DEEPER_RETVAL_ERROR;
      }
   }

   do
   {
      PrefetchWaitCrcMsg waitMsg(std::string(path), cachePath, deeper_prefetch_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &waitMsg,
         NETMSGTYPE_CachePrefetchCrcWaitResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (PrefetchWaitCrcRespMsg*)respMsg;
      cacheErrorCode = respMsgCast->getError();

      if(cacheErrorCode == EBUSY)
      {
         SAFE_DELETE(respMsg);
         SAFE_FREE(respBuf);

         // wait a few seconds before trying again
         ::sleep(DEEPERCACHE_WAIT_SLEEP_SEC);
      }
   } while (cacheErrorCode == EBUSY);

   if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Wait for asynchronous prefetch with CRC checksum calculation failed on path: " +
         std::string(path) + "; errno: " + System::getErrString(cacheErrorCode) );
      errno = cacheErrorCode;
   }
   else
   {
      *outChecksum = respMsgCast->getChecksum();
      retVal = DEEPER_RETVAL_SUCCESS;
   }


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Checks if the prefetch of a the given path is finished. Checks the prefetch operation from
 * deeper_cache_prefetch[_range | _crc]().
 *
 * NOTE: The function doesn't report errors from the prefetch. To get the error is a additional wait
 *       required.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags The flags which was used for the prefetch.
 * @return DEEPER_IS_NOT_FINISHED If prefetch is ongoing or DEEPER_IS_FINISHED if prefetch is
 *         finished or DEEPER_RETVAL_ERROR in case of error.
 */
int DeeperCache::cache_prefetch_is_finished(const char* path, int deeper_prefetch_flags)
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   int retVal = DEEPER_RETVAL_ERROR;
   std::string cachePath(path);

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   PrefetchIsFinishedRespMsg* respMsgCast;

   PrefetchIsFinishedMsg isFinishedMsg(std::string(path), cachePath, deeper_prefetch_flags);

   commRes = MessagingTk::requestResponse(getLocalNode(), &isFinishedMsg,
      NETMSGTYPE_CachePrefetchIsFinishedResp, &respBuf, &respMsg);
   if(!commRes)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Communication with cache daemon failed. path: " + std::string(path) );
      goto err_cleanup;
   }

   respMsgCast = (PrefetchIsFinishedRespMsg*)respMsg;

   retVal = respMsgCast->getValue();


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Stops an ongoing prefetch operation from deeper_cache_prefetch[_range | _crc]() to complete.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags The flags which was used for the prefetch.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_prefetch_stop(const char* path, int deeper_prefetch_flags)
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   int retVal = DEEPER_RETVAL_ERROR;
   int cacheErrorCode = DEEPER_RETVAL_ERROR;
   std::string cachePath(path);

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   PrefetchStopRespMsg* respMsgCast;

   PrefetchStopMsg stopMsg(std::string(path), cachePath, deeper_prefetch_flags);

   commRes = MessagingTk::requestResponse(getLocalNode(), &stopMsg,
      NETMSGTYPE_CachePrefetchStopResp, &respBuf, &respMsg);
   if(!commRes)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Communication with cache daemon failed. path: " + std::string(path) );
      goto err_cleanup;
   }

   respMsgCast = (PrefetchStopRespMsg*)respMsg;

   cacheErrorCode = respMsgCast->getValue();
   if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Couldn't stop asynchronous prefetch on path: " + std::string(path) +
         "; errno: " + System::getErrString(cacheErrorCode) );
      errno = cacheErrorCode;
   }
   else
   {
      retVal = DEEPER_RETVAL_SUCCESS;
   }


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Flush a file from the current cache domain to global storage, asynchronously. Contents of an
 * existing file with the same name on global storage will be overwritten.
 *
 * @param path Path to file, which should be flushed.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT To make this a synchronous operation, which means return only after
 *           flushing is complete.
 *        DEEPER_FLUSH_SUBDIRS To recursively copy all subdirs, if given path leads to a
 *           directory.
 *        DEEPER_FLUSH_DISCARD To remove the file from the cache layer after it has been flushed.
 *        DEEPER_FLUSH_FOLLOWSYMLINKS To copy the destination file or directory and do not create
 *           symbolic links when a symbolic link was found.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_flush(const char* path, int deeper_flush_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + cachePath);
#endif

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }


   if(!(deeper_flush_flags & DEEPER_FLUSH_WAIT) )
   { // the asynchronous implementation
      int cacheErrorCode = DEEPER_RETVAL_ERROR;

      bool commRes;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      FlushRespMsg* respMsgCast;

      FlushMsg flushMsg(cachePath, std::string(path), deeper_flush_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &flushMsg,
         NETMSGTYPE_CacheFlushResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (FlushRespMsg*)respMsg;

      cacheErrorCode = respMsgCast->getValue();
      if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Wait for asynchronous flush failed on path: " + std::string(path) +
            "; errno: " + System::getErrString(cacheErrorCode) );
         errno = cacheErrorCode;
      }
      else
      {
         retVal = DEEPER_RETVAL_SUCCESS;
      }


err_cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);
   }
   else if(deeper_flush_flags & DEEPER_FLUSH_SUBDIRS)
   { // the synchronous implementation for directory traversal
      retVal = filesystemtk::handleSubdirectoryFlag(cachePath.c_str(), path, false,
         deeper_flush_flags & DEEPER_FLUSH_DISCARD,
         deeper_flush_flags & DEEPER_FLUSH_FOLLOWSYMLINKS);
   }
   else
   { // the synchronous implementation
      if(!cachepathtk::createPath(path, app->getConfig()->getSysMountPointCache(),
         app->getConfig()->getSysMountPointGlobal(), true, Logger::getLogger() ) )
         return DEEPER_RETVAL_ERROR;

      struct stat statSource;
      int funcError = lstat(cachePath.c_str(), &statSource);
      if(funcError != 0)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Could not stat source file/directory: " + cachePath + " (cache FS); "
            "errno: " + System::getErrString(errno) );
         return DEEPER_RETVAL_ERROR;
      }

      if(S_ISLNK(statSource.st_mode) )
      {
         if(deeper_flush_flags & DEEPER_FLUSH_FOLLOWSYMLINKS)
         {
            threadvartk::setThreadKeyNftwFollowSymlink(true);

            retVal = filesystemtk::handleFollowSymlink(cachePath.c_str(), path, &statSource, false,
               deeper_flush_flags & DEEPER_FLUSH_DISCARD, true, false, NULL);

            //reset thread variables
            threadvartk::setThreadKeyNftwFollowSymlink(false);
            threadvartk::resetThreadKeyNftwNumFollowedSymlinks();
         }
         else
         {
            retVal = filesystemtk::createSymlink(cachePath.c_str(), path, &statSource, false,
               deeper_flush_flags & DEEPER_FLUSH_DISCARD);
         }
      }
      else if(S_ISREG(statSource.st_mode) )
      {
         retVal = filesystemtk::copyFile(cachePath.c_str(), path, &statSource,
            deeper_flush_flags & DEEPER_FLUSH_DISCARD, false, NULL, NULL);
      }
      else if(S_ISDIR(statSource.st_mode) )
      {
         errno = EISDIR;
         Logger::getLogger()->logErr(__FUNCTION__,
            std::string("Given path is an directory but DEEPER_FLUSH_SUBDIRS flag is not set: ") +
            path);
         retVal = DEEPER_RETVAL_ERROR;
      }
      else
      {
         errno = EINVAL;
         Logger::getLogger()->logErr(__FUNCTION__,
            std::string("Unsupported file type for path: ") + path);
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
 * @param path Path to file, which should be flushed.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT To make this a synchronous operation, which means return only after
 *           flushing is complete.
 *        DEEPER_FLUSH_DISCARD To remove the file from the cache layer after it has been flushed.
 *        DEEPER_FLUSH_FOLLOWSYMLINKS To copy the destination file or directory and do not create
 *           symbolic links when a symbolic link was found.
 * @param outChecksum The checksum of the file, it is only used in the synchronous case.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_flush_crc(const char* path, int deeper_flush_flags,
   unsigned long* outChecksum)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + cachePath);
#endif

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   if(!cachepathtk::createPath(path, app->getConfig()->getSysMountPointCache(),
      app->getConfig()->getSysMountPointGlobal(), true, Logger::getLogger() ) )
      return DEEPER_RETVAL_ERROR;


   if(!(deeper_flush_flags & DEEPER_FLUSH_WAIT) )
   { // the asynchronous implementation
      int cacheErrorCode = DEEPER_RETVAL_ERROR;

      bool commRes;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      FlushCrcRespMsg* respMsgCast;

      FlushCrcMsg flushCrcMsg(cachePath, std::string(path), deeper_flush_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &flushCrcMsg,
         NETMSGTYPE_CacheFlushCrcResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (FlushCrcRespMsg*)respMsg;

      cacheErrorCode = respMsgCast->getValue();
      if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Couldn't submit asynchronous flush with CRC checksum calculation on path: " +
            std::string(path) + "; errno: " + System::getErrString(cacheErrorCode) );
         errno = cacheErrorCode;
      }
      else
      {
         retVal = DEEPER_RETVAL_SUCCESS;
      }


err_cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);

      return retVal;
   }


   // the synchronous implementation
   struct stat statSource;
   int funcError = lstat(cachePath.c_str(), &statSource);
   if(funcError != 0)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Could not stat source file/directory: " + cachePath + " (cache FS); errno: " +
         System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   if(S_ISREG(statSource.st_mode) )
   {
      retVal = filesystemtk::copyFile(cachePath.c_str(), path, &statSource,
         deeper_flush_flags & DEEPER_FLUSH_DISCARD, true, NULL, outChecksum);
   }
   else if(S_ISLNK(statSource.st_mode) )
   {
      if(deeper_flush_flags & DEEPER_FLUSH_FOLLOWSYMLINKS)
      {
         threadvartk::setThreadKeyNftwFollowSymlink(true);

         retVal = filesystemtk::handleFollowSymlink(cachePath.c_str(), path, &statSource, false,
            deeper_flush_flags & DEEPER_FLUSH_DISCARD, true, true, outChecksum);

         //reset thread variables
         threadvartk::setThreadKeyNftwFollowSymlink(false);
         threadvartk::resetThreadKeyNftwNumFollowedSymlinks();
      }
      else
      {
         errno = ENOLINK;
         Logger::getLogger()->logErr(__FUNCTION__, std::string("Given path is a symlink. "
            "CRC checksum calculation requires a file or the DEEPER_FLUSH_FOLLOWSYMLINKS flag: ") +
            path);
         retVal = DEEPER_RETVAL_ERROR;
      }
   }
   else if(S_ISDIR(statSource.st_mode) )
   {
      errno = EISDIR;
      Logger::getLogger()->logErr(__FUNCTION__, std::string("Given path is an directory. "
         "CRC checksum calculation doesn't support directories: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   {
      errno = EINVAL;
      Logger::getLogger()->logErr(__FUNCTION__,
         std::string("Unsupported file type for path: ") + path);
      retVal = DEEPER_RETVAL_ERROR;
   }

   return retVal;
}

/**
 * Flush a file similar to deeper_cache_flush(), but flush only a certain range, not the whole file.
 *
 * @param path Path to file, which should be flushed.
 * @param pos Start position (offset) of the byte range that should be flushed.
 * @param num_bytes Number of bytes from pos that should be flushed.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT to make this a synchronous operation.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_flush_range(const char* path, off_t start_pos, size_t num_bytes,
   int deeper_flush_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + cachePath);
#endif

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }


   if(!(deeper_flush_flags & DEEPER_FLUSH_WAIT) )
   { // the asynchronous implementation
      int cacheErrorCode = DEEPER_RETVAL_ERROR;

      bool commRes;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      FlushRangeRespMsg* respMsgCast;

      FlushRangeMsg flushRange(cachePath, std::string(path), deeper_flush_flags, start_pos,
         num_bytes);

      commRes = MessagingTk::requestResponse(getLocalNode(), &flushRange,
         NETMSGTYPE_CacheFlushRangeResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (FlushRangeRespMsg*)respMsg;

      cacheErrorCode = respMsgCast->getValue();
      if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Couldn't submit asynchronous range flush on path: " + std::string(path) +
            "; errno: " + System::getErrString(cacheErrorCode) );
      }
      else
      {
         retVal = DEEPER_RETVAL_SUCCESS;
      }


err_cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);

      return retVal;
   }


   // the synchronous implementation
   if(!cachepathtk::createPath(path, app->getConfig()->getSysMountPointCache(),
      app->getConfig()->getSysMountPointGlobal(), true, Logger::getLogger() ) )
         return DEEPER_RETVAL_ERROR;

   retVal = filesystemtk::copyFileRange(cachePath.c_str(), path, NULL, &start_pos, num_bytes, NULL,
      true);

   return retVal;
}

/**
 * Wait for an ongoing flush operation from deeper_cache_flush[_range]() to complete.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_SUBDIRS To recursively wait contents of all subdirs, if given path leads
 *           to a directory.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_flush_wait(const char* path, int deeper_flush_flags)
{
   int retVal = DEEPER_RETVAL_ERROR;
   int cacheErrorCode = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   FlushWaitRespMsg* respMsgCast;

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   // check if the given path exists in the cache FS or in the global FS, if not there couldn't be
   // a flush operation, check also the destination path, because the flush could have the
   // DEEPER_FLUSH_DISCARD flag
   struct stat statSource;
   int funcError = lstat(cachePath.c_str(), &statSource);
   if(funcError != 0)
   {
      struct stat statDest;
      funcError = lstat(path, &statDest);
      if(funcError != 0)
      {
         Logger::getLogger()->logErr(__FUNCTION__, "Could not stat source file/directory: " +
            cachePath + " and the destination file/directory " + path +
            ". No ongoing flush for the given path possible. "
            "errno: " + System::getErrString(errno) );
         errno = EINVAL;
         return DEEPER_RETVAL_ERROR;
      }
   }

   do
   {
      FlushWaitMsg waitMsg(cachePath, std::string(path), deeper_flush_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &waitMsg,
         NETMSGTYPE_CacheFlushWaitResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (FlushWaitRespMsg*)respMsg;
      cacheErrorCode = respMsgCast->getValue();

      if(cacheErrorCode == EBUSY)
      {
         SAFE_DELETE(respMsg);
         SAFE_FREE(respBuf);

         // wait a few seconds before trying again
         ::sleep(DEEPERCACHE_WAIT_SLEEP_SEC);
      }
   } while (cacheErrorCode == EBUSY);

   if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Wait for asynchronous flush failed on path: " + std::string(path) +
         "; errno: " + System::getErrString(cacheErrorCode) );
      errno = cacheErrorCode;
   }
   else
   {
      retVal = DEEPER_RETVAL_SUCCESS;
   }


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Wait for an ongoing flush operation from deeper_cache_flush_crc() to complete.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param outChecksum The checksum of the file.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE. Currently ignored, but maybe later some flags
 *           required.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_flush_wait_crc(const char* path, int deeper_flush_flags,
   unsigned long* outChecksum)
{
   int retVal = DEEPER_RETVAL_ERROR;
   int cacheErrorCode = DEEPER_RETVAL_ERROR;

   std::string cachePath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   FlushWaitCrcRespMsg* respMsgCast;

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   // check if the given path exists in the cache FS or in the global FS, if not there couldn't be
   // a flush operation, check also the destination path, because the flush could have the
   // DEEPER_FLUSH_DISCARD flag
   struct stat statSource;
   int funcError = lstat(cachePath.c_str(), &statSource);
   if(funcError != 0)
   {
      struct stat statDest;
      funcError = lstat(path, &statDest);
      if(funcError != 0)
      {
         Logger::getLogger()->logErr(__FUNCTION__, "Could not stat source file/directory " +
            cachePath + " and the destination file/directory " + path +
            ". No ongoing flush for the given path possible. "
            "errno: " + System::getErrString(errno) );
         errno = EINVAL;
         return DEEPER_RETVAL_ERROR;
      }
   }

   do
   {
      FlushWaitCrcMsg waitMsg(cachePath, std::string(path), deeper_flush_flags);

      commRes = MessagingTk::requestResponse(getLocalNode(), &waitMsg,
         NETMSGTYPE_CacheFlushCrcWaitResp, &respBuf, &respMsg);
      if(!commRes)
      {
         Logger::getLogger()->logErr(__FUNCTION__,
            "Communication with cache daemon failed. path: " + std::string(path) );
         goto err_cleanup;
      }

      respMsgCast = (FlushWaitCrcRespMsg*)respMsg;
      cacheErrorCode = respMsgCast->getError();

      if(cacheErrorCode == EBUSY)
      {
         SAFE_DELETE(respMsg);
         SAFE_FREE(respBuf);

         // wait a few seconds before trying again
         ::sleep(DEEPERCACHE_WAIT_SLEEP_SEC);
      }
   } while (cacheErrorCode == EBUSY);

   if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Wait for asynchronous flush with CRC checksum calculation failed on path: " +
         std::string(path) + "; errno: " + System::getErrString(cacheErrorCode) );
      errno = cacheErrorCode;
   }
   else
   {
      *outChecksum = respMsgCast->getChecksum();
      retVal = DEEPER_RETVAL_SUCCESS;
   }


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Checks if the flush of a the given path is finished. Checks the flush operation from
 * deeper_cache_flush[_range | _crc]().
 *
 * NOTE: The function doesn't report errors from the flush. To get the error is a additional wait
 *       required.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param deeper_flush_flags The flags which was used for the flush.
 * @return DEEPER_IS_NOT_FINISHED If flush is ongoing or DEEPER_IS_FINISHED if flush is finished or
 *         DEEPER_RETVAL_ERROR in case of error.
 */
int DeeperCache::cache_flush_is_finished(const char* path, int deeper_flush_flags)
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   int retVal = DEEPER_RETVAL_ERROR;
   std::string cachePath(path);
   std::string destPath(path);

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   FlushIsFinishedRespMsg* respMsgCast;

   FlushIsFinishedMsg isFinishedMsg(cachePath, destPath, deeper_flush_flags);

   commRes = MessagingTk::requestResponse(getLocalNode(), &isFinishedMsg,
      NETMSGTYPE_CacheFlushIsFinishedResp, &respBuf, &respMsg);
   if(!commRes)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Communication with cache daemon failed. path: " + std::string(path) );
      goto err_cleanup;
   }

   respMsgCast = (FlushIsFinishedRespMsg*)respMsg;
   retVal = respMsgCast->getValue();


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Stops an ongoing flush operation from deeper_cache_flush[_range | _crc]() to complete.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param deeper_flush_flags The flags which was used for the flush.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_flush_stop(const char* path, int deeper_flush_flags)
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + std::string(path) );
#endif

   int retVal = DEEPER_RETVAL_ERROR;
   int cacheErrorCode = DEEPER_RETVAL_ERROR;
   std::string cachePath(path);
   std::string destPath(path);

   if(!cachepathtk::globalPathToCachePath(cachePath, app->getConfig(), Logger::getLogger() ) )
   {
      errno = EINVAL;
      return retVal;
   }

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   FlushStopRespMsg* respMsgCast;

   FlushStopMsg stopMsg(cachePath, destPath, deeper_flush_flags);

   commRes = MessagingTk::requestResponse(getLocalNode(), &stopMsg,
      NETMSGTYPE_CacheFlushStopResp, &respBuf, &respMsg);
   if(!commRes)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Communication with cache daemon failed. path: " + std::string(path) );
      goto err_cleanup;
   }

   respMsgCast = (FlushStopRespMsg*)respMsg;

   cacheErrorCode = respMsgCast->getValue();
   if(cacheErrorCode != DEEPER_RETVAL_SUCCESS)
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "Couldn't stop asynchronous flush on path: " + std::string(path) +
         "; errno: " + System::getErrString(cacheErrorCode) );
      errno = cacheErrorCode;
   }
   else
   {
      retVal = DEEPER_RETVAL_SUCCESS;
   }


err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * Return the stat information of a file or directory of the cache domain.
 *
 * @param path To a file or directory on the global file system.
 * @param out_stat_data The stat information of the file or directory.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_stat(const char *path, struct stat *out_stat_data)
{
   int retVal = DEEPER_RETVAL_ERROR;
   std::string newPath(path);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + newPath);
#endif

   if(cachepathtk::globalPathToCachePath(newPath, app->getConfig(), Logger::getLogger() ) )
      retVal = stat(newPath.c_str(), out_stat_data);
   else
      errno = EINVAL;

   return retVal;
}

/**
 * Return a unique identifier for the current cache domain.
 *
 * @param out_cache_id pointer To a buffer in which the ID of the current cache domain will be
 *        stored on success.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int DeeperCache::cache_id(const char* path, uint64_t* out_cache_id)
{
   std::string newPath(path);

   cachepathtk::preparePaths(newPath);

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path: " + newPath);
#endif

   if(newPath[0] != '/')
   {
      if(cachepathtk::makePathAbsolute(newPath, Logger::getLogger() ) )
         return DEEPER_RETVAL_ERROR;
   }

   if(!cachepathtk::isRootPath(newPath, app->getConfig()->getSysMountPointGlobal() ) )
   {
      Logger::getLogger()->logErr(__FUNCTION__,
         "The given path doesn't point to a global BeeGFS: " + std::string(path) +
         "; The cache API requires a path to the global BeeGFS.");
      errno = EINVAL;
      return DEEPER_RETVAL_ERROR;
   }

   *out_cache_id = app->getConfig()->getSysCacheID();

   return DEEPER_RETVAL_SUCCESS;
}



/**
 * Add file descriptor with the deeper_open_flags to the map.
 *
 * @param fd The fd of the file.
 * @param deeper_open_flags The deeper_open_flags which was used during the open.
 * @param path The path of the file.
 */
void DeeperCache::addEntryToSessionMap(int fd, int deeper_open_flags, std::string path)
{
   SafeMutexLock lock(&fdMapMutex);                         // W R I T E L O C K

   DeeperCacheSession session(fd, deeper_open_flags, path);
   std::pair<DeeperCacheSessionMapIter, bool> pair = this->sessionMap.insert(
      DeeperCacheSessionMapVal(fd, session) );

   if(!(pair.second) )
      Logger::getLogger()->logErr(__FUNCTION__,
         "Could not add FD to session map. FD: " + StringTk::intToStr(fd) +
         "; path: " + path);

   lock.unlock();                                           // U N L O C K
}

/**
 * Get the DeeperCacheSession for a given file descriptor and remove the session from the map.
 *
 * @param inOutSession The session with the searched FD and after calling this method it contains
 *                     all required information.
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
      Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Could not find FD in session map: " +
         StringTk::intToStr(inOutSession.getFD() ) );

   lock.unlock();                                           // U N L O C K
}
