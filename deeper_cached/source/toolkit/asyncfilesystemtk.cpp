#include <app/config/Config.h>
#include <common/app/log/Logger.h>
#include <common/cache/toolkit/cachepathtk.h>
#include <common/cache/toolkit/filesystemtk.h>
#include <common/cache/toolkit/threadvartk.h>
#include <components/worker/CopyFileCrcWork.h>
#include <components/worker/CopyFileWork.h>
#include <components/worker/DiscardWork.h>
#include <components/worker/FollowSymlinkWork.h>
#include <components/worker/ProcessDirWork.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>

#include <libgen.h>

#include "asyncfilesystemtk.h"



/**
 * Copies a directory asynchronous to an other directory using.
 *
 * @param source The path to the source directory.
 * @param dest The path to the destination directory.
 * @param deeperFlags The deeper flags DEEPER_PREFETCH_... or DEEPER_FLUSH_....
 * @param type The type of the work package, a value of CacheWorkType.
 * @param rootWork The root work of the work package.
 * @param followSymlinkCounter The number of followed symlinks before this function call.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int asyncfilesystemtk::copyDir(const char* source, const char* dest, int deeperFlags,
   CacheWorkType type, CacheWork* rootWork, int followSymlinkCounter)
{
   App* app = Program::getApp();
   Config* config = app->getConfig();
   Logger* log = Logger::getLogger();

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + std::string(source) +
      "; dest path: " + dest);
#endif

   int retVal = DEEPER_RETVAL_SUCCESS;

   bool copyToCache;
   bool followSymlink;
   bool deleteSource;
   int deeperFlagsFile;

   if(type == CacheWorkType_PREFETCH)
   {
      copyToCache = true;
      followSymlink = deeperFlags & DEEPER_PREFETCH_FOLLOWSYMLINKS;
      deleteSource = false;
      deeperFlagsFile = deeperFlags & (~DEEPER_FLUSH_SUBDIRS);
   }
   else
   {
      copyToCache = false;
      followSymlink = deeperFlags & DEEPER_FLUSH_FOLLOWSYMLINKS;
      deleteSource = deeperFlags & DEEPER_FLUSH_DISCARD;
      deeperFlagsFile = deeperFlags & (~DEEPER_FLUSH_SUBDIRS);
   }

   const std::string& mountOfSource(copyToCache ? config->getSysMountPointGlobal() :
      config->getSysMountPointCache() );
   const std::string& mountOfDest(copyToCache ? config->getSysMountPointCache() :
      config->getSysMountPointGlobal() );

   DIR *dir;
   dirent *ent;
   struct stat statSource;

   retVal = stat(source, &statSource);
   if(retVal == DEEPER_RETVAL_ERROR)
   {
      log->log(Log_ERR, __FUNCTION__, "Could not stat source directory: " + std::string(source) +
         "; errno: " + System::getErrString(errno) );

      return DEEPER_RETVAL_ERROR;
   }

   dir = opendir(source);
   if(dir)
   {
      if(!cachepathtk::createPath(dest, mountOfSource, mountOfDest, false, log) )
         return DEEPER_RETVAL_ERROR;

      retVal = chown(dest, statSource.st_uid, statSource.st_gid);
      if( (retVal == DEEPER_RETVAL_ERROR) && (errno != EPERM) )
      {
         log->log(Log_ERR, __FUNCTION__, "Could not set owner of directory: " + std::string(dest) +
            "; errno: " + System::getErrString(errno) );

         return retVal;
      }
   }
   else
   {
      log->log(Log_ERR, __FUNCTION__, "Could not open source directory: " + std::string(source) +
         "; errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   while ( (ent = readdir(dir) ) != NULL)
   {
      const std::string fileName = ent->d_name;

      if( (fileName.size() == 1) && (fileName[0] == '.') )
         continue; // skip directory itself
      if( (fileName.size() == 2) && (fileName[0] == '.') && (fileName[1] == '.') )
         continue; // skip parent directory

      const std::string newSourcePath(source + std::string("/") +  fileName);
      const std::string newDestPath(dest + std::string("/") + fileName);

      int funcError = lstat(newSourcePath.c_str(), &statSource);
      if(funcError != 0)
      {
         log->log(Log_ERR, __FUNCTION__, "Could not stat source file/directory " + newSourcePath +
            "; errno: " + System::getErrString(errno) );
         retVal = DEEPER_RETVAL_ERROR;
      }
      else if(S_ISREG(statSource.st_mode) )
      {
         Work* work = new CopyFileWork(newSourcePath, newDestPath, deeperFlagsFile, type,
            rootWork, NULL, followSymlinkCounter);
         retVal = Program::getApp()->getWorkQueue()->addWork(work, false);
      }
      else if(S_ISLNK(statSource.st_mode) )
      {
         if(!cachepathtk::createPath(newDestPath, mountOfSource, mountOfDest, true, log) )
         {
            retVal = DEEPER_RETVAL_ERROR;
         }
         else
         {
            if(followSymlink)
            {
               Work* work = new FollowSymlinkWork(newSourcePath, newDestPath, deeperFlags, type,
                  rootWork, followSymlinkCounter);
               retVal = Program::getApp()->getWorkQueue()->addWork(work, false);
            }
            else
            {
               retVal = filesystemtk::createSymlink(newSourcePath.c_str(), newDestPath.c_str(),
                  &statSource, copyToCache, deleteSource);
            }
         }
      }
      else if(S_ISDIR(statSource.st_mode) )
      {
         Work* work = new ProcessDirWork(newSourcePath, newDestPath, deeperFlags, type,
            rootWork, followSymlinkCounter);
         retVal = Program::getApp()->getWorkQueue()->addWork(work, false);
      }
      else
      {
         log->log(Log_ERR, __FUNCTION__, "Unsupported entry in directory: " + std::string(source) );
      }
   }
   closedir(dir);

   return retVal;
}

/**
 * Copies a directory asynchronous to an other directory, also do cleanup the source directory using
 * nftw().
 *
 * @param source The path to the source directory.
 * @param dest The path to the destination directory.
 * @param deeperFlags The deeper flags DEEPER_PREFETCH_... or DEEPER_FLUSH_....
 * @param type The type of the work package, a value of CacheWorkType.
 * @param rootWork The root work of the work package.
 * @param isRootWork Set to true if the function is called from the root work. If true this work
 *        takes care about the delete of the file system tree
 * @param followSymlinkCounter The number of followed symlinks before this function call.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int asyncfilesystemtk::handleSubdirectoryFlag(const char* source, const char* dest,
   int deeperFlags, CacheWorkType type, CacheWork* rootWork, bool isRootWork,
   int followSymlinkCounter)
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Source path: " + std::string(source) + "; dest path: " + dest );
#endif

   if(type == CacheWorkType_FLUSH)
   {
      if(isRootWork && (deeperFlags & DEEPER_FLUSH_DISCARD) )
         rootWork->setDoDiscardForRoot();

      if(deeperFlags & DEEPER_FLUSH_FOLLOWSYMLINKS)
         deeperFlags = deeperFlags & (~DEEPER_FLUSH_DISCARD);
   }

   // copy the directory, if the discard flag is given to copy dir depends on the followSymlink flag
   int retVal = asyncfilesystemtk::copyDir(source, dest, deeperFlags, type, rootWork,
      followSymlinkCounter);

   if(retVal != DEEPER_RETVAL_SUCCESS)
   {
      errno = retVal;
      retVal = DEEPER_RETVAL_ERROR;
   }

   return retVal;
}

/**
 * Handles the followSymlink flag.
 *
 * @param sourcePath The source path.
 * @param destPath The destination path.
 * @param statSource The stat of the symlink.
 * @param copyToCache True if the data should be copied from the global BeeGFS to the cache
 *        BeeGFS, false if the data should be copied from the cache BeeGFS to the global BeeGFS.
 * @param deleteSource True if the file/directory should be deleted after the file/directory was
 *        copied.
 * @param ignoreDir True if directories should be ignored, should be set if the flag
 *        DEEPER_..._SUBDIRS wasn't used.
 * @param doCRC True if a CRC checksum should be calculated.
 * @param deeperFlags The deeper flags DEEPER_PREFETCH_... or DEEPER_FLUSH_....
 * @param type The type of the work package, a value of CacheWorkType.
 * @param rootWork The root work of the work package.
 * @param followSymlinkCounter The number of followed symlinks before this function call.
 * @param crcOutValue Out value for the checksum if a checksum should be calculated.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int asyncfilesystemtk::handleFollowSymlink(std::string sourcePath, std::string destPath,
   const struct stat* statSource, bool copyToCache, bool deleteSource, bool ignoreDir,
   bool doCRC, int deeperFlags, CacheWorkType type, CacheWork* rootWork,
   int followSymlinkCounter, unsigned long* crcOutValue)
{
   App* app = Program::getApp();
   Config* config = app->getConfig();
   Logger* log = Logger::getLogger();

   int retVal = DEEPER_RETVAL_ERROR;

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + sourcePath + "; dest path: " + destPath);
#endif

   // get the new source path from the symlink value
   std::string pathFromLink;
   if(!cachepathtk::readLink(sourcePath.c_str(), statSource, log, pathFromLink) )
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
      if(!cachepathtk::isGlobalPath(pathFromLink, config) )
      {
         log->log(Log_ERR, __FUNCTION__, "Link points outside of global FS: " + pathFromLink);

         return DEEPER_RETVAL_SUCCESS; // Do not report as an error, only log it to console
      }
   }
   else
   {
      if(!cachepathtk::isCachePath(pathFromLink, config) )
      {
         log->log(Log_ERR, __FUNCTION__, "Link points outside of cache FS: " + pathFromLink);

         return DEEPER_RETVAL_SUCCESS; // Do not report as an error, only log it to console
      }
   }

   struct stat statNewSourcePath;
   retVal = lstat(pathFromLink.c_str(), &statNewSourcePath);

   if(retVal == DEEPER_RETVAL_ERROR)
   {
      log->log(Log_ERR, __FUNCTION__, "Could not stat source file: " + pathFromLink +
         "; errno: " + System::getErrString(errno) );

      return DEEPER_RETVAL_ERROR;
   }

   // check if the maximum number of symlinks to follow is reached
   if(followSymlinkCounter <= filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS)
   {
      ++followSymlinkCounter;
#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + sourcePath + "; dest path: " + destPath +
      std::string("; number of followed symlinks: ") + StringTk::intToStr(followSymlinkCounter) +
      std::string("; max allowed symlinks to follow ") +
      StringTk::intToStr(filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS) );
#endif
   }
   else
   {
      errno = ELOOP;

#ifndef BEEGFS_DEBUG
      log->log(Log_ERR, __FUNCTION__, "Too many symbolic links were encountered: " + sourcePath +
         "; errno: " + System::getErrString(errno) );
#else
      log->log(Log_ERR, __FUNCTION__, "Too many symbolic links were encountered: " + sourcePath +
         "; errno: " + System::getErrString(errno) +
         std::string("; number of followed symlinks: ") + StringTk::intToStr(followSymlinkCounter) +
         std::string("; max allowed symlinks to follow ") +
         StringTk::intToStr(filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS) );
#endif

      return DEEPER_RETVAL_ERROR;
   }

   if(S_ISDIR(statNewSourcePath.st_mode) )
   {
      if(unlikely(ignoreDir || doCRC) )
      {
         errno = EISDIR;
         log->log(Log_ERR, __FUNCTION__, "Symlink points to directory, but the flag "
            "DEEPER_..._FOLLOWSYMLINKS is set and the flag DEEPER_..._SUBDIRS is not set: "
            + pathFromLink + "; errno: " + System::getErrString(errno) );

         retVal = DEEPER_RETVAL_ERROR;
      }
      else
      {
         Work* work = new ProcessDirWork(pathFromLink, destPath, deeperFlags, type, rootWork,
            followSymlinkCounter);
         retVal = app->getWorkQueue()->addWork(work, false);
      }
   }
   else if(S_ISREG(statNewSourcePath.st_mode) )
   {
      if(doCRC)
      {
         Work* work = new CopyFileCrcWork(pathFromLink, destPath, deeperFlags, type, rootWork);
         retVal = Program::getApp()->getWorkQueue()->addWork(work, false);
      }
      else
      {
         Work* work = new CopyFileWork(pathFromLink, destPath, deeperFlags, type, rootWork,
            NULL, followSymlinkCounter);
         retVal = Program::getApp()->getWorkQueue()->addWork(work, false);
      }
   }
   else if(S_ISLNK(statNewSourcePath.st_mode) )
   {
      retVal = asyncfilesystemtk::handleFollowSymlink(pathFromLink.c_str(), destPath,
         &statNewSourcePath, copyToCache, deleteSource, ignoreDir, doCRC, deeperFlags, type,
         rootWork, followSymlinkCounter, crcOutValue);
   }
   else
   {
      errno = EINVAL;
      log->log(Log_ERR, __FUNCTION__, "Unexpected typeflag value for path: " + pathFromLink);

      retVal = DEEPER_RETVAL_ERROR;
   }

   return retVal;
}

/**
 * Calculates the number of prefetch or flush work packages to copy a file in parallel.
 *
 * @param minBlockSize The block size for each work to copy.
 * @param fileSize The size of the file to split.
 * @return The number of work packages on success, DEEPER_RETVAL_ERROR if minBlockSize is zero.
 */
int asyncfilesystemtk::calculateNumCopyThreads(uint64_t minBlockSize, uint64_t fileSize)
{
   if(!minBlockSize)
      return DEEPER_RETVAL_ERROR;

   int retVal = fileSize / minBlockSize;

   if( (retVal * minBlockSize) < fileSize)
      retVal++;

   return retVal;
}

/**
 * Do cleanup of the source directory. The root directory is processed by opendir and for each
 * sub-directory a cache work is created which process all entries and sub-directories by nftw().
 *
 * @param sourcePath The path to the source directory.
 * @param deeperFlags The deeper flags DEEPER_PREFETCH_... or DEEPER_FLUSH_....
 * @param rootWork The root work of the work package.
 * @param rootWorkIsLocked True if the root work is currently locked.
 * @param outNewDeleteWork Set to true if works submitted to delete the sub-directories.
 * @return DEEPER_RETVAL_SUCCESS on success, else the errno.
 */
int asyncfilesystemtk::doDelete(const char* sourcePath, int deeperFlags, CacheWork* rootWork,
   bool rootWorkIsLocked, bool &outNewDeleteWork)
{
   Logger* log = Logger::getLogger();

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Source path: " + std::string(sourcePath) +
      "; deeper flags: " + StringTk::intToStr(deeperFlags) );
#endif

   int retVal = DEEPER_RETVAL_SUCCESS;
   bool doFollowSymlink = deeperFlags & DEEPER_FLUSH_FOLLOWSYMLINKS;

   DIR *dir;
   dirent *ent;
   struct stat statSource;

   dir = opendir(sourcePath);
   if(!dir)
   {
      log->log(Log_ERR, __FUNCTION__, "Could not open directory to delete directory: " +
         std::string(sourcePath) + "; errno: " + System::getErrString(errno) );
      return DEEPER_RETVAL_ERROR;
   }

   while ( (ent = readdir(dir) ) != NULL)
   {
      const std::string fileName = ent->d_name;

      if( (fileName.size() == 1) && (fileName[0] == '.') )
         continue; // skip directory itself
      if( (fileName.size() == 2) && (fileName[0] == '.') && (fileName[1] == '.') )
         continue; // skip parent directory

      const std::string newSourcePath(sourcePath + std::string("/") +  fileName);

      int funcError = lstat(newSourcePath.c_str(), &statSource);
      if(funcError != 0)
      {
         log->log(Log_ERR, __FUNCTION__, "Could not stat source file/directory " + newSourcePath +
            "; errno: " + System::getErrString(errno) );
         retVal = DEEPER_RETVAL_ERROR;
      }
      else if(S_ISLNK(statSource.st_mode) || S_ISREG(statSource.st_mode) )
      {
         if(unlikely(!doFollowSymlink) )
         {
            log->log(Log_ERR, __FUNCTION__, "Found a file or symlink " + newSourcePath +
               ", but all files and symlinks should be deleted.");
            retVal = DEEPER_RETVAL_ERROR;
         }
         else if(unlink(newSourcePath.c_str() ) )
         {
            log->log(Log_ERR, __FUNCTION__, "Could not delete file after copy. path: " +
               newSourcePath + "; errno: " + System::getErrString(errno) );
            retVal = DEEPER_RETVAL_ERROR;
         }
      }
      else if(S_ISDIR(statSource.st_mode) )
      {
         outNewDeleteWork = true;
         Work* work = new DiscardWork(newSourcePath, deeperFlags, rootWork, rootWorkIsLocked);
         retVal = Program::getApp()->getWorkQueue()->addWork(work, false);
      }
      else
      {
         log->log(Log_ERR, __FUNCTION__, "Unsupported entry in directory: " +
            std::string(newSourcePath) );
      }
   }
   closedir(dir);

   return retVal;
}
