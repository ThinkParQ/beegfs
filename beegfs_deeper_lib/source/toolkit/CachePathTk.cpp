#include "CachePathTk.h"



/**
 * checks a path if the root path is equals to a given path
 *
 * @path the path to check
 * @param rootPath the expected root path of the given path
 * @return true if the given path has the expected root path
 */
bool CachePathTk::isRootPath(const std::string& path, const std::string& rootPath)
{
   size_t startPos = path.find(rootPath);
   if(startPos)
      return false;

   return true;
}

/**
 * checks if a path point to a file/directory on the global BeeGFS, requires an absolute path
 *
 * @param path the path to test
 * @param deeperLibConfig the config of the beegfs deeper lib
 * @return true if the path point to a file/directory on the global BeeGFS, false if not
 */
bool CachePathTk::isGlobalPath(const std::string& path, const Config* deeperLibConfig)
{
   return CachePathTk::isRootPath(path, deeperLibConfig->getSysMountPointGlobal() );
}

/**
 * checks if a path point to a file/directory on the cache BeeGFS, requires an absolute path
 *
 * @param path the path to test
 * @param deeperLibConfig the config of the beegfs deeper lib
 * @return true if the path point to a file/directory on the global BeeGFS, false if not
 */
bool CachePathTk::isCachePath(const std::string& path, const Config* deeperLibConfig)
{
   return CachePathTk::isRootPath(path, deeperLibConfig->getSysMountPointCache() );
}

/**
 * replaces the mount point path of the global BeeGFS with the mount point path of the cache BeeGFS
 *
 * @param inOutPath the path to a file or directory on the global BeeGFS
 * @param deeperLibConfig the config of the beegfs deeper lib
 * @param log the logger to log error messages
 * @return true on success, false if the path is not pointing to the global BeeGFS
 */
bool CachePathTk::globalPathToCachePath(std::string& inOutPath, const Config* deeperLibConfig,
   Logger* log)
{
#ifdef BEEGFS_DEBUG
      log->logErr(__FUNCTION__, "path: " + inOutPath);
#endif

   return replaceRootPath(inOutPath, deeperLibConfig->getSysMountPointGlobal(),
      deeperLibConfig->getSysMountPointCache(), false, log);
}

/**
 * replaces the mount point path of the cache BeeGFS with the mount point path of the global BeeGFS
 *
 * @param inOutPath the path to a file or directory on the cache BeeGFS
 * @param deeperLibConfig the config of the beegfs deeper lib
 * @param log the logger to log error messages
 * @return true on success, false if the path is not pointing to the cache BeeGFS
 */
bool CachePathTk::cachePathToGlobalPath(std::string& inOutPath, const Config* deeperLibConfig,
   Logger* log)
{
#ifdef BEEGFS_DEBUG
      log->logErr(__FUNCTION__, "path: " + inOutPath);
#endif

   return replaceRootPath(inOutPath, deeperLibConfig->getSysMountPointCache(),
      deeperLibConfig->getSysMountPointGlobal(), false, log);
}

/**
 * replaces a part of the given path beginning from the root, it also converts a relative path to
 * an absolute path
 *
 * @param inOutPath the path to modify
 * @param origPath the expected root part of the path which needs to replaced
 * @param newPath the new root part of the path
 * @param canonicalizePaths true if the paths must be canonicalized, before the paths can be used
 * @param log the logger to log error messages
 * @return true on success, false if the root path part is not matching to the expected root path
 *         part
 */
bool CachePathTk::replaceRootPath(std::string& inOutPath, const std::string& origPath,
   const std::string& newPath, bool canonicalizePaths, Logger* log)
{
   if(canonicalizePaths)
   {
      std::string origPathCanonicalized(origPath);
      std::string newPathCanonicalized(newPath);

      preparePaths(inOutPath);
      preparePaths(origPathCanonicalized);
      preparePaths(newPathCanonicalized);

      return CachePathTk::replaceRootPath(inOutPath, origPathCanonicalized, newPathCanonicalized,
         false, log);
   }

   if(inOutPath[0] != '/')
   {
      if(!makePathAbsolute(inOutPath, log) )
         return false;
   }

   if(CachePathTk::isRootPath(inOutPath, origPath) )
      inOutPath.replace(0, origPath.length(), newPath);
   else
   {
      log->logErr(__FUNCTION__, "The given path " + inOutPath + " is not located on the "
         "mount " + origPath);

      return false;
   }

#ifdef BEEGFS_DEBUG
      log->logErr(__FUNCTION__, "path: " + inOutPath);
#endif

   return true;
}

/**
 * modifies the given path from a relative path to an absolute path
 *
 * @param inOutPath the relative path, after this method the path is absolute
 * @param log the logger to log error messages
 * @return true on success, false on error, ernno is set
 */
bool CachePathTk::makePathAbsolute(std::string& inOutPath, Logger* log)
{
   bool retVal = true;
   char* realPath = (char*) malloc(PATH_MAX);

#ifdef BEEGFS_DEBUG
      log->logErr(__FUNCTION__, "in path: " + inOutPath);
#endif

   char *res = realpath(inOutPath.c_str(), realPath);
   if(res)
   {
      inOutPath.assign((const char *) realPath);

#ifdef BEEGFS_DEBUG
      log->logErr(__FUNCTION__, "out path: " + inOutPath);
#endif
   }
   else
   if(errno == ENOENT)
   {
      if(inOutPath.empty() )
         return false;

      // remove trailing /
      if(inOutPath[inOutPath.size() -1] == '/')
         inOutPath.resize(inOutPath.size() -1);

      // remove last path element from path and store it for later use
      size_t indexLast = inOutPath.find_last_of("/");
      std::string removedPart(inOutPath.substr(indexLast + 1) );
      inOutPath.resize(indexLast + 1);

      // find a valid parent with recursive method calls
      if(makePathAbsolute(inOutPath, log) )
         inOutPath.append("/" + removedPart); // rebuild path after recursive method calls
      else
         log->logErr(__FUNCTION__, "Couldn't convert given path to an absolute path. "
            "Given path: " + inOutPath + " (" + System::getErrString(errno) + ")");
   }
   else
   {
      log->logErr(__FUNCTION__, "Couldn't convert given path to an absolute path. "
         "Given path: " + inOutPath + " (" + System::getErrString(errno) + ")");
      retVal = false;
   }

   SAFE_FREE(realPath);

   return retVal;
}

/**
 * modifies the given path from a relative path to an absolute path, with the given start directory
 * of the relative path
 *
 * @param inOutPath the relative path, after this method the path is absolute
 * @param startDirOfPath the start path of the relative path
 * @param log the logger to log error messages
 * @return true on success, false on error, ernno is set
 */
bool CachePathTk::makePathAbsolute(std::string& inOutPath, const std::string& startDirOfPath,
   Logger* log)
{
   inOutPath.insert(0, startDirOfPath + '/');
   return CachePathTk::makePathAbsolute(inOutPath, log);
}

/**
 * Create path directories. The owner and the mode is used from a source directory.
 *
 * Note: Existing directories are ignored (i.e. not an error).
 *
 * @param destPath the path to create
 * @param sourceMount the mount point of the source path to clone (keep mode and owner)
 * @param destMount the mount point of the path to create
 * @param excluseLastElement true if the last element should not be created.
 * @param log the logger to log error messages
 * @return true on success, false otherwise and errno is set from mkdir() in this case.
 */
bool CachePathTk::createPath(std::string destPath, std::string sourceMount,
   std::string destMount, bool excludeLastElement, Logger* log)
{
#ifdef BEEGFS_DEBUG
      log->logErr(__FUNCTION__, "path: " + destPath);
#endif

   struct stat sourceStat;

   if(destMount[destMount.size() -1] != '/')
      destMount += "/";
   if(sourceMount[sourceMount.size() -1] != '/')
      sourceMount += "/";

   // remove mount points from the path to get a relative path from mount
   destPath.erase(0, destMount.size() );

   StringList pathElems;
   StringTk::explode(destPath,'/', &pathElems);

   unsigned pathCreateDepth = excludeLastElement ? pathElems.size() -1 : pathElems.size();

   int mkdirErrno = 0;

   StringListConstIter iter = pathElems.begin();
   for(unsigned i=0; i < pathCreateDepth; iter++, i++)
   {
      sourceMount += *iter;
      destMount += *iter;

      int statRes = stat(sourceMount.c_str(), &sourceStat);
      if(statRes)
      {
         log->logErr(__FUNCTION__, "Couldn't stat source directory: " + sourceMount +
            " (" + System::getErrString(errno) + ")");
         return false;
      }

      int mkRes = mkdir(destMount.c_str(), sourceStat.st_mode);
      mkdirErrno = errno;
      if(mkRes && (mkdirErrno == EEXIST) )
      { // path exists already => check whether it is a directory
         struct stat statStruct;
         int statRes = stat(destMount.c_str(), &statStruct);
         if(statRes || !S_ISDIR(statStruct.st_mode) )
         {
            log->logErr(__FUNCTION__, "A element of the path is not a directory: " +
               destMount + " (" + System::getErrString(errno) + ")");
            return false;
         }

      }
      else
      if(mkRes)
      {
         log->logErr(__FUNCTION__, "Couldn't not create path: " + destMount +
            " (" + System::getErrString(errno) + ")");
         return false;
      }

      int chownRes = chown(destMount.c_str(), sourceStat.st_uid, sourceStat.st_gid);
      if(chownRes && (errno != EPERM) )
      { // EPERM happens when the process is not running as root, this can be ignored
         log->logErr(__FUNCTION__, "Couldn't not change owner of directory: " +
            destMount + " (" + System::getErrString(errno) + ")");
         return false;
      }

      // prepare next loop
      sourceMount += "/";
      destMount += "/";
   }

   if (mkdirErrno == EEXIST)
   {
      // ensure the right path permissions for the last element
      int chmodRes = chmod(destMount.c_str(), sourceStat.st_mode);
      if(chmodRes)
         log->log(Log_DEBUG, __FUNCTION__, "Couldn't not change mode of directory: "
            + destMount + " (" + System::getErrString(errno) + ")");
      // ensure the right path owner for the last element
      int chownRes = chown(destMount.c_str(), sourceStat.st_uid, sourceStat.st_gid);
      if(chownRes && (errno != EPERM) )
         log->log(Log_DEBUG, __FUNCTION__, "Couldn't not change owner of directory: " +
            destMount + " (" + System::getErrString(errno) + ")");
   }

   return true;
}

/**
 * Create path directories. The owner and the mode is used from a source directory.
 *
 * Note: Existing directories are ignored (i.e. not an error).
 *
 * @param destPath the path to create
 * @param sourceMount the mount point of the source path to clone (keep mode and owner)
 * @param destMount the mount point of the path to create
 * @param excluseLastElement true if the last element should not be created.
 * @param log the logger to log error messages
 * @return true on success, false otherwise and errno is set from mkdir() in this case.
 */
bool CachePathTk::createPath(const char* destPath, const std::string& sourceMount,
   const std::string& destMount, bool excludeLastElement, Logger* log)
{
   std::string newDestPath(destPath);

   return CachePathTk::createPath(newDestPath, sourceMount, destMount, excludeLastElement, log);
}


bool CachePathTk::readLink(const char* sourcePath, const struct stat* statSource,
   std::string& outLinkContent, Logger* log)
{
   int internalRetVal;
   size_t linkSize = 0;

   if(!statSource || !S_ISLNK(statSource->st_mode) )
   {
      struct stat newStatSource;
      internalRetVal = lstat(sourcePath, &newStatSource);

      if(internalRetVal)
      {
         log->logErr(__FUNCTION__, "Could not stat source file: " +
         std::string(sourcePath) + " Errno: " + System::getErrString(errno) );

         return false;
      }

      linkSize = newStatSource.st_size;
   }
   else
      linkSize = statSource->st_size;


   char* linkname = (char*) malloc(linkSize + 1);

   if(linkname != NULL)
      internalRetVal = readlink(sourcePath, linkname, linkSize + 1);
   else
   {
      log->logErr("handleFlags", "Could not malloc buffer for readlink " +
         std::string(sourcePath) + " Errno: " + System::getErrString(errno) );

      return false;
   }

   if(internalRetVal < 0)
   {
      SAFE_FREE(linkname);

      log->logErr("handleFlags", "Could not readlink data " + std::string(sourcePath) +
         " Errno: " + System::getErrString(errno) );

      return false;
   }
   else if(linkSize < (size_t)internalRetVal)
   {
      SAFE_FREE(linkname);

      log->logErr("handleFlags", "symlink increased in size between lstat() "
         "and readlink()" + std::string(sourcePath) );

      return false;
   }
   else
      linkname[linkSize] = '\0';

   outLinkContent.assign(linkname);

   SAFE_FREE(linkname);

   return true;
}

bool CachePathTk::bothCharactersAreSlashes(char a, char b)
{
   return (a == '/') && (b == '/');
}

void CachePathTk::preparePaths(std::string& inOutString)
{
   std::string::iterator iterUnique = std::unique(inOutString.begin(), inOutString.end(),
      bothCharactersAreSlashes);

   std::string::iterator iterRemove = std::remove(inOutString.begin(), iterUnique, ' ');

   if(*--iterRemove == '/')
      inOutString.erase(iterRemove, inOutString.end() );
   else
      inOutString.erase(++iterRemove, inOutString.end() );
}
