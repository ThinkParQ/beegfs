#include "MapTk.h"
#include "StorageTk.h"
#include "System.h"

#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/statfs.h>


#define STORAGETK_DIR_LOCK_FILENAME          "lock.pid"

#define STORAGETK_FORMAT_FILENAME            "format.conf"
#define STORAGETK_FORMAT_KEY_VERSION         "version"


bool StorageTk::createPathOnDisk(Path& path, bool excludeLastElement)
{
   const StringList* pathElems = path.getPathElems();
   std::string pathStr = path.isAbsolute() ? "/" : "";
   unsigned pathCreateDepth = excludeLastElement ? pathElems->size() -1 : pathElems->size();

   StringListConstIter iter = pathElems->begin();
   for(unsigned i=0; i < pathCreateDepth; iter++, i++)
   {
      pathStr += *iter;

      int mkRes = mkdir(pathStr.c_str(), 0777);
      if(mkRes && (errno == EEXIST) )
      { // path exists already => check whether it is a directory
         struct stat statStruct;
         int statRes = stat(pathStr.c_str(), &statStruct);
         if(statRes || !S_ISDIR(statStruct.st_mode) )
            return false;
      }
      else
      if(mkRes)
      { // error
         return false;
      }

      // all right
      pathStr += "/";
   }

   return true;
}

/**
 * @param keepDepth the 1-based depth of the path that should not be deleted
 */
bool StorageTk::removePathDirsFromDisk(Path& path, unsigned keepDepth)
{
   const StringList* pathElems = path.getPathElems();
   std::string pathStr = path.isAbsolute() ? "/" : "";
   return removePathDirsRec(pathElems->begin(), pathStr, 1, pathElems->size(), keepDepth);
}

/**
 * @param currentDepth 1-based path depth
 */
bool StorageTk::removePathDirsRec(StringListConstIter dirNameIter, std::string pathStr,
   unsigned currentDepth, unsigned numPathElems, unsigned keepDepth)
{
   pathStr += *dirNameIter;

   if(currentDepth < numPathElems)
   { // we're not at the end yet
      if(!removePathDirsRec(++StringListConstIter(dirNameIter), pathStr+"/",
         currentDepth+1, numPathElems, keepDepth) )
         return false;
   }

   if(currentDepth <= keepDepth)
      return true;

   int rmRes = rmdir(pathStr.c_str() );

   return(!rmRes || (errno == ENOENT) );
}

/**
 * Retrieve free/used space info for a mounted file system based on a path to a (sub)directory of
 * the mount point.
 *
 * @return false on error (and errno is set in this case)
 */
bool StorageTk::statStoragePath(const std::string path, int64_t* outSizeTotal, int64_t* outSizeFree,
   int64_t* outInodesTotal, int64_t* outInodesFree)
{
   /* note: don't use statvfs() here, because it requires a special lookup in "/proc" for the
      f_flag field. */

   struct statfs statBuf;

   int statRes = statfs(path.c_str(), &statBuf);

   if(statRes)
      return false;

   // success => copy out-values

   *outSizeTotal = statBuf.f_blocks * statBuf.f_bsize;
   *outSizeFree = statBuf.f_bavail * statBuf.f_bsize;
   *outInodesTotal = statBuf.f_files;
   *outInodesFree = statBuf.f_ffree;

   return true;
}


/**
 * Note: Works by non-exclusively creating of a lock file (incl. path if necessary) and then locking
 * it non-blocking via flock(2).
 * Note: Remember to call unlock later (if this method succeeded).
 * Note: The PID inside the file might no longer be up-to-date if this is called before a daemon
 * forks to background mode, so don't rely on it.
 *
 * @param pathStr path to a directory (not including a filename)
 * @return -1 on error, >=0 (valid fd) on success
 */
int StorageTk::lockWorkingDirectory(std::string pathStr)
{
   Path dirPath(pathStr);
   bool createPathRes = StorageTk::createPathOnDisk(dirPath, false);
   if(!createPathRes)
   {
      int errCode = errno;
      std::cerr << "Unable to create working directory: " << pathStr << "; " <<
         "SysErr: " << System::getErrString(errCode) << std::endl;

      return -1;
   }

   Path filePath(dirPath, STORAGETK_DIR_LOCK_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

   int fd = createAndLockPIDFile(filePathStr, true);
   if(fd == -1)
   {
      std::cerr << "Unable to lock working directory. Check if the directory is already in use: " <<
         pathStr << std::endl;
   }

   return fd;
}

/**
 * @param fd the fd you got from lockWorkingDirectory()
 */
void StorageTk::unlockWorkingDirectory(int fd)
{
   unlockPIDFile(fd);
}

/**
 * Note: Works by non-exclusively creating of a pid file and then trying to lock it non-blocking via
 * flock(2).
 * Note: Remember to call unlock later (if this method succeeded).
 *
 * @param pathStr path (including a filename) of the pid file, parent dirs will not be created
 * @param writePIDToFile whether or not you want to write the PID to the file (if false, the file
 * will just be truncated to 0 size); false is useful before a daemon forked to background mode.
 * @return -1 on error, >=0 (valid fd) on success
 */
int StorageTk::createAndLockPIDFile(std::string pathStr, bool writePIDToFile)
{
   int fd = open(pathStr.c_str(), O_CREAT | O_WRONLY, 0664);
   if(fd == -1)
   { // error
      int errCode = errno;

      std::cerr << "Unable to create/open working directory lock file: " << pathStr << "; "
         << "SysErr: " << System::getErrString(errCode) << std::endl;

      return -1;
   }

   int flockRes = flock(fd, LOCK_EX | LOCK_NB);
   if(flockRes == -1)
   {
      int errCode = errno;

      if(errCode == EWOULDBLOCK)
      { // file already locked
         std::cerr << "Unable to lock PID file: " << pathStr << " (already locked)." << std::endl <<
            "Check if service is already running." << std::endl;
      }
      else
      { // error
         std::cerr << "Unable to lock PID file: " << pathStr << "; " <<
            "SysErr: " << System::getErrString(errCode) << std::endl;
      }

      close(fd);
      return -1;
   }

   if(writePIDToFile)
   { // update file with current PID
      bool updateRes = updateLockedPIDFile(fd);
      if(updateRes == -1)
      {
         std::cerr << "Problem encountered during PID file update: " << pathStr << std::endl;

         flock(fd, LOCK_UN);
         close(fd);
         return -1;
      }
   }
   else
   { // just truncate the file (and don't write PID)
      int truncRes = ftruncate(fd, 0);
      if(truncRes == -1)
      { // error
         int errCode = errno;

         std::cerr << "Unable to truncate PID file. FD: " << fd << "; "
            << "SysErr: " << System::getErrString(errCode) << std::endl;

         return false;
      }
   }

   return fd;
}

/**
 * Updates the pid file with the current pid. Typically required after calling daemon().
 *
 * Note: Make sure the file was already locked by createAndLockPIDFile() when calling this.
 */
bool StorageTk::updateLockedPIDFile(int fd)
{
   int truncRes = ftruncate(fd, 0);
   if(truncRes == -1)
   { // error
      int errCode = errno;

      std::cerr << "Unable to truncate PID file. FD: " << fd << "; "
         << "SysErr: " << System::getErrString(errCode) << std::endl;

      return false;
   }

   std::string pidStr = StringTk::uint64ToStr(System::getPID() );
   pidStr += '\n'; // add newline, because some shell tools don't like it when there is no newline

   ssize_t writeRes = pwrite(fd, pidStr.c_str(), pidStr.length(), 0);
   if(writeRes != (ssize_t)pidStr.length() )
   { // error
      int errCode = errno;

      std::cerr << "Unable to write to PID file. FD: " << fd << "; "
         << "SysErr: " << System::getErrString(errCode) << std::endl;

      return false;
   }

   fsync(fd);

   return true;
}


/**
 * @param fd the fd you got from createAndLockPIDFile()
 */
void StorageTk::unlockPIDFile(int fd)
{
   flock(fd, LOCK_UN);
   close(fd);
}


/**
 * Note: Creates the file only if it does not exist yet.
 *
 * @param pathStr path to the main storage working directory (not including a filename)
 * @param formatProperties arbitrary key/value pairs that should be stored in the format file
 * (may be NULL); does not need to contain formatVersion key/value.
 * @return true if format file was created or existed already
 */
bool StorageTk::createStorageFormatFile(const std::string pathStr, int formatVersion,
   StringMap* formatProperties)
{
   Path dirPath(pathStr);
   Path filePath(dirPath, STORAGETK_FORMAT_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

   /* note: we must not touch an existing format file to make sure we don't overwrite any values
      that are meant to be persistent */
   if(pathExists(filePathStr.c_str() ) )
      return true;

   StringMap formatFileMap;

   if(formatProperties)
      formatFileMap = *formatProperties;

   std::string warningStr = "# This file was auto-generated. Do not modify it!";

   formatFileMap[warningStr] = "";
   formatFileMap[STORAGETK_FORMAT_KEY_VERSION] = StringTk::intToStr(formatVersion);

   bool saveRes = MapTk::saveStringMapToFile(filePathStr.c_str(), &formatFileMap);
   if(!saveRes)
   {
      std::cerr << "Unable to save storage format file. " <<
         "SysErr: " << System::getErrString() << std::endl;

      return false;
   }

   return true;
}

/**
 * @param pathStr path to the main storage working directory (not including a filename)
 * @param outFormatProperties all key/value pairs that were stored in the format file (may be NULL).
 * @return false if format file was not valid (eg didn't exist or contained wrong version).
 */
bool StorageTk::checkStorageFormatFile(const std::string pathStr, int formatVersion,
   StringMap* outFormatProperties)
{
   Path dirPath(pathStr);
   Path filePath(dirPath, STORAGETK_FORMAT_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

   StringMap formatFileMap;

   MapTk::loadStringMapFromFile(filePathStr.c_str(), &formatFileMap);

   if(formatFileMap.empty() )
   {
      std::cout << "Storage format file is empty: " << filePathStr;
      return false;
   }

   StringMapIter iter = formatFileMap.find(STORAGETK_FORMAT_KEY_VERSION);
   if(iter == formatFileMap.end() )
   {
      std::cout << "Storage format file does not contain version info: " << filePathStr;
      return false;
   }

   if(StringTk::strToInt(iter->second.c_str() ) != formatVersion)
   {
      std::cout << "Incompatible storage format: " << iter->second << " (required: " <<
         formatVersion << ")" << std::endl;
      return false;
   }

   if(outFormatProperties)
      outFormatProperties->swap(formatFileMap);

   return true;
}

/**
 * Note: This is intended to check e.g. whether a storage directory has been initialized already.
 *
 * @param pathStr path to the main storage working directory (not including a filename)
 * @return true if format file exists already
 */
bool StorageTk::checkStorageFormatFileExists(const std::string pathStr)
{
   Path dirPath(pathStr);
   Path filePath(dirPath, STORAGETK_FORMAT_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

   return pathExists(filePathStr.c_str() );
}


/**
 * Reads the old targetID for the given path or generates a new one and stores it.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 * @param targetID the ID that should be assigned to this path
 */
bool StorageTk::createTargetIDFile(const std::string pathStr, const std::string targetID)
{
   // check if file exists already and read it, otherwise create it with new targetID

   Path metaPath(pathStr);
   Path targetIDPath(metaPath, STORAGETK_TARGETID_FILENAME);

   int fd = open(targetIDPath.getPathAsStr().c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
   if(fd == -1)
   { // error
      std::ostringstream outStream;
      outStream << "Unable to create file for storage target ID: " << targetIDPath.getPathAsStr()
         << "; " << "SysErr: " << System::getErrString();

      std::cout << outStream.str() << std::endl;

      return false;
   }

   ssize_t writeRes = write(fd, targetID.c_str(), targetID.length() );
   if(writeRes != (ssize_t)targetID.length() )
   { // error
      std::ostringstream outStream;
      outStream << "Unable to write targetID file: " << targetIDPath.getPathAsStr()
         << "; " << "SysErr: " << System::getErrString();

      close(fd);

      std::cout << outStream.str() << std::endl;

      return false;
   }

   fsync(fd);
   close(fd);

   return true;
}


/**
 * Calls system readdir() and skips entries "." and ".."
 *
 * @return same as system readdir() except that this will never return the entries "." and ".."
 */
struct dirent* StorageTk::readdirFiltered(DIR* dirp)
{
      for( ; ; )
      {
         struct dirent* dirEntry = readdir(dirp);
         if(!dirEntry)
         {
            return dirEntry;
         }
         else
         if(strcmp(dirEntry->d_name, ".") &&
            strcmp(dirEntry->d_name, "..") )
         {
            return dirEntry;
         }
      }
}

