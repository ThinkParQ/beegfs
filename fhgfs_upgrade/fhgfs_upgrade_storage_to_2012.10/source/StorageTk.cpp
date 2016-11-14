#include "MapTk.h"
#include "StorageTk.h"
#include "System.h"
#include "ICommonConfig.h"

#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/statfs.h>
#include "LogContext.h"


#define STORAGETK_DIR_LOCK_FILENAME          "lock.pid"

#define STORAGETK_FORMAT_FILENAME            "format.conf"
#define STORAGETK_FORMAT_KEY_VERSION         "version"

/**
 * Init hash sub directories.
 */
bool StorageTk::initHashPaths(Path& basePath, int maxLevel1, int maxLevel2)
{
   const char* logContext = "Initialize Hash Paths";

   // entries subdirs
   std::string level1LastName = StringTk::uintToHexStr(maxLevel1 - 1);
   Path level1LastSubDirPath(basePath, level1LastName);

   std::string level2LastName = StringTk::uintToHexStr(maxLevel2 - 1);
   Path level2LastSubDirPath(level1LastSubDirPath, level2LastName);

   if(!StorageTk::pathExists(level2LastSubDirPath.getPathAsStr() ) )
   { // last subdir does not exist yet, so we need to create the dirs

      for (int level1=0; level1 < maxLevel1; level1++)
      {
         std::string level1SubDirName = StringTk::uintToHexStr(level1);
         Path level1SubDirPath(basePath, level1SubDirName);

         if(!StorageTk::createPathOnDisk(level1SubDirPath, false) )
         {
            LogContext(logContext).logErr("Unable to create subdir: " +
               level1SubDirPath.getPathAsStr() );
            return false;
         }

         for (int level2=0; level2 < maxLevel2; level2++)
         {
            std::string level2SubDirName = StringTk::uintToHexStr(level2);
            Path level2SubDirPath(level1SubDirPath, level2SubDirName);

            if(!StorageTk::createPathOnDisk(level2SubDirPath, false) )
            {
               LogContext(logContext).logErr("Unable to create subdir: " +
                  level2SubDirPath.getPathAsStr() );
               return false;
            }
         }
      }
   }

   return true;
}


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
   const char* logContext = "Lock the working directory";
   Path dirPath(pathStr);
   bool createPathRes = StorageTk::createPathOnDisk(dirPath, false);
   if(!createPathRes)
   {
      int errCode = errno;
      LogContext(logContext).logErr("Unable to create working directory: " + pathStr + "; "
         "SysErr: " + System::getErrString(errCode) );

      return -1;
   }

   Path filePath(dirPath, STORAGETK_DIR_LOCK_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

   int fd = createAndLockPIDFile(filePathStr, true);
   if(fd == -1)
   {
      LogContext(logContext).logErr("Unable to lock working directory. "
         "Check if the directory is already in use: " + pathStr);
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
   const char* logContext = "Create and lock pid file";
   int fd = open(pathStr.c_str(), O_CREAT | O_WRONLY, 0664);
   if(fd == -1)
   { // error
      int errCode = errno;

      LogContext(logContext).logErr("Unable to create/open working directory lock file: "
         + pathStr + "; SysErr: " + System::getErrString(errCode) );

      return -1;
   }

   int flockRes = flock(fd, LOCK_EX | LOCK_NB);
   if(flockRes == -1)
   {
      int errCode = errno;

      if(errCode == EWOULDBLOCK)
      { // file already locked
         LogContext(logContext).logErr("Unable to lock PID file: " + pathStr +
            " (already locked). Check if service is already running." );
      }
      else
      { // error
         LogContext(logContext).logErr("Unable to lock PID file: " + pathStr + "; "
            "SysErr: " + System::getErrString(errCode) );
      }

      close(fd);
      return -1;
   }

   if(writePIDToFile)
   { // update file with current PID
      bool updateRes = updateLockedPIDFile(fd);
      if(updateRes == -1)
      {
         LogContext(logContext).logErr("Problem encountered during PID file update: " + pathStr);

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

         LogContext(logContext).logErr("Unable to truncate PID file. FD: "
            + StringTk::intToStr(fd) +  "SysErr: " + System::getErrString(errCode) );

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
   const char* logContext = "Update pid file";

   int truncRes = ftruncate(fd, 0);
   if(truncRes == -1)
   { // error
      int errCode = errno;

      LogContext(logContext).logErr("Unable to truncate PID file. FD: " + StringTk::intToStr(fd) +
         "; SysErr: " + System::getErrString(errCode) );

      return false;
   }

   std::string pidStr = StringTk::uint64ToStr(System::getPID() );
   pidStr += '\n'; // add newline, because some shell tools don't like it when there is no newline

   ssize_t writeRes = pwrite(fd, pidStr.c_str(), pidStr.length(), 0);
   if(writeRes != (ssize_t)pidStr.length() )
   { // error
      int errCode = errno;

      LogContext(logContext).logErr("Unable to write to PID file. FD: " + StringTk::intToStr(fd) +
         "; SysErr: " + System::getErrString(errCode) );

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
   const char* logContext = "Create storage format file";
   Path dirPath(pathStr);
   Path filePath(dirPath, STORAGETK_FORMAT_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

#if 0 // disabled for upgrade tool
   /* note: we must not touch an existing format file to make sure we don't overwrite any values
      that are meant to be persistent */
   if(pathExists(filePathStr.c_str() ) )
      return true;
#endif

   StringMap formatFileMap;

   if(formatProperties)
      formatFileMap = *formatProperties;

   std::string warningStr = "# This file was auto-generated. Do not modify it!";

   formatFileMap[warningStr] = "";
   formatFileMap[STORAGETK_FORMAT_KEY_VERSION] = StringTk::intToStr(formatVersion);

   bool saveRes = MapTk::saveStringMapToFile(filePathStr.c_str(), &formatFileMap);
   if(!saveRes)
   {
      LogContext(logContext).logErr("Unable to save storage format file. "
         "SysErr: " + System::getErrString() );

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
   const char* logContext = "Check storage format file";
   Path dirPath(pathStr);
   Path filePath(dirPath, STORAGETK_FORMAT_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

   StringMap formatFileMap;

   MapTk::loadStringMapFromFile(filePathStr.c_str(), &formatFileMap);

   if(formatFileMap.empty() )
   {
      LogContext(logContext).logErr("Storage format file is empty: " + filePathStr);
      return false;
   }

   StringMapIter iter = formatFileMap.find(STORAGETK_FORMAT_KEY_VERSION);
   if(iter == formatFileMap.end() )
   {
      LogContext(logContext).logErr("Storage format file does not contain version info: " +
         filePathStr);
      return false;
   }

   if(StringTk::strToInt(iter->second.c_str() ) != formatVersion)
   {
      LogContext(logContext).logErr("Incompatible storage format: " + iter->second +
         " (required: " + StringTk::intToStr(formatVersion) + ")" );
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
 * Reads the old targetID for the given path.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
bool StorageTk::readTargetIDFile(const std::string pathStr, std::string* outTargetID)

{
   // check if file exists already and read it, otherwise create it with new targetID

   Path storageDirPath(pathStr);
   Path targetIDPath(storageDirPath, STORAGETK_TARGETID_FILENAME);

   StringList targetIDList; // actually, the file would contain only a single line

   bool targetIDPathExists = StorageTk::pathExists(targetIDPath.getPathAsStr() );
   if(!targetIDPathExists)
      return false;

   bool loadRes = ICommonConfig::loadStringListFile(targetIDPath.getPathAsStr().c_str(),
      targetIDList);
   if (!loadRes)
      return false;

   std::string oldTargetID;

   if(!targetIDList.empty() )
      oldTargetID = *targetIDList.begin();

   if(oldTargetID.empty() )
      return false;

   *outTargetID = oldTargetID;
   return true;
}

/**
 * Reads the old targetID for the given path.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
bool StorageTk::readNodeIDFile(const std::string pathStr, std::string fileName,
   std::string& outNodeID)

{
   // check if file exists already and read it, otherwise create it with new targetID

   Path storageDirPath(pathStr);
   Path nodeIDPath(storageDirPath, fileName);

   StringList nodeIDList; // actually, the file would contain only a single line

   bool nodeIDPathExists = StorageTk::pathExists(nodeIDPath.getPathAsStr() );
   if(!nodeIDPathExists)
      return false;

   bool loadRes = ICommonConfig::loadStringListFile(nodeIDPath.getPathAsStr().c_str(),
      nodeIDList);
   if (!loadRes)
      return false;

   std::string nodeID;

   if(!nodeIDList.empty() )
      nodeID = *nodeIDList.begin();

   if(nodeID.empty() )
      return false;

   outNodeID = nodeID;
   return true;
}

/**
 * Create file for numeric target ID and write given ID into it.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
bool StorageTk::createNumIDFile(const std::string pathStr, const std::string filename,
   uint16_t targetID)
{
   const char* logContext = "Create numeric ID file";
   Path storageDirPath(pathStr);
   Path targetIDPath(storageDirPath, filename);

   std::string targetIDPathTmp = targetIDPath.getPathAsStr() + STORAGETK_TMPFILE_EXT;

   std::string newTargetIDStr = StringTk::uintToStr(targetID);

   // create tmp file with new targetID

   int fd = open(targetIDPathTmp.c_str(), O_CREAT | O_TRUNC | O_RDWR, STORAGETK_DEFAULT_FILEMODE);
   if(fd == -1)
   { // error
      LogContext(logContext).logErr("Unable to create file for storage target ID: " +
         targetIDPathTmp +  ". " +  "SysErr: " + System::getErrString() );

      return false;
   }

   std::string newTargetIDFileStr = newTargetIDStr + "\n";
   ssize_t writeRes = write(fd, newTargetIDFileStr.c_str(), newTargetIDFileStr.length() );
   if(writeRes != (ssize_t)newTargetIDFileStr.length() )
   { // error
      LogContext(logContext).logErr("Unable to write targetID file: " + targetIDPathTmp + ". " +
         "SysErr: " + System::getErrString() );

      close(fd);

      return false;
   }

   fsync(fd);
   close(fd);

   // rename tmp file to its actual name

   int renameRes = rename(targetIDPathTmp.c_str(), targetIDPath.getPathAsStr().c_str() );
   if(renameRes == -1)
   { // rename failed
      LogContext(logContext).logErr("Unable to rename numeric ID file: " + targetIDPathTmp + ". " +
         "SysErr: " + System::getErrString() );
      return false;
   }

   return true;
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

   const char* logContext = "Create numeric ID file";

   Path metaPath(pathStr);
   Path targetIDPath(metaPath, STORAGETK_TARGETID_FILENAME);

   int fd = open(targetIDPath.getPathAsStr().c_str(), O_CREAT | O_TRUNC | O_RDWR, 0664);
   if(fd == -1)
   { // error
      std::ostringstream outStream;
      outStream << "Unable to create file for storage target ID: " << targetIDPath.getPathAsStr()
         << "; " << "SysErr: " << System::getErrString();

      LogContext(logContext).logErr(outStream.str() );

      return false;
   }

   ssize_t writeRes = write(fd, targetID.c_str(), targetID.length() );
   if(writeRes != (ssize_t)targetID.length() )
   { // error
      std::ostringstream outStream;
      outStream << "Unable to write targetID file: " << targetIDPath.getPathAsStr()
         << "; " << "SysErr: " << System::getErrString();

      close(fd);

      LogContext(logContext).logErr(outStream.str() );

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

