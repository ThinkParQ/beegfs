#include <common/app/config/ICommonConfig.h>
#include <common/toolkit/MapTk.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/TimeAbs.h>
#include <common/toolkit/UnitTk.h>
#include <common/storage/Metadata.h>
#include "StorageTk.h"

#include <ftw.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/statfs.h>
#include <libgen.h>

#define STORAGETK_DIR_LOCK_FILENAME          "lock.pid"

#define STORAGETK_FORMAT_FILENAME            "format.conf"
#define STORAGETK_FORMAT_KEY_VERSION         "version"

#define STORAGETK_TMPFILE_EXT                ".tmp" /* tmp extension for saved files until rename */
#define STORAGETK_BACKUP_EXT                 ".bak" /* extension for old file backups */

#define STORAGETK_DEFAULT_FILEMODE           (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

#define STORAGETK_DEFAULT_DIRMODE           (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

#define STORAGETK_FREESPACE_FILENAME         "free_space.override"
#define STORAGETK_FREEINODES_FILENAME        "free_inodes.override"

#ifndef BTRFS_SUPER_MAGIC
#define BTRFS_SUPER_MAGIC 0x9123683E /* <linux/magic.h>, which isn't available on older distros */
#endif


/* see generateFileID() for comments */
AtomicUInt64 StorageTk::idCounter(
   System::getCurrentTimeSecs() << STORAGETK_FILEID_TIMESTAMP_SHIFTBITS);


/**
 * Init hash sub directories.
 */
void StorageTk::initHashPaths(Path& basePath, int maxLevel1, int maxLevel2)
{
   // inodes directores

   // entries subdirs
   std::string level1LastName = StringTk::uintToHexStr(maxLevel1 - 1);
   Path level1LastSubDirPath = basePath / level1LastName;

   std::string level2LastName = StringTk::uintToHexStr(maxLevel2 - 1);
   Path level2LastSubDirPath = level1LastSubDirPath / level2LastName;

   if(!StorageTk::pathExists(level2LastSubDirPath.str() ) )
   { // last subdir does not exist yet, so we need to create the dirs

      for (int level1=0; level1 < maxLevel1; level1++)
      {
         std::string level1SubDirName = StringTk::uintToHexStr(level1);
         Path level1SubDirPath = basePath / level1SubDirName;

         if(!StorageTk::createPathOnDisk(level1SubDirPath, false) )
            throw InvalidConfigException("Unable to create subdir: " +
               level1SubDirPath.str() );

         for (int level2=0; level2 < maxLevel2; level2++)
         {
            std::string level2SubDirName = StringTk::uintToHexStr(level2);
            Path level2SubDirPath = level1SubDirPath / level2SubDirName;

            if(!StorageTk::createPathOnDisk(level2SubDirPath, false) )
               throw InvalidConfigException("Unable to create subdir: " +
                  level2SubDirPath.str() );
         }
      }
   }

}

/**
 * Create path directories.
 *
 * Note: Existing directories are ignored (i.e. not an error).
 *
 * @param excluseLastElement true if the last element should not be created.
 * @param inMode: NULL unless the caller sets it
 * @return true on success, false otherwise and errno is set from mkdir() in this case.
 */
bool StorageTk::createPathOnDisk(const Path& path, bool excludeLastElement, mode_t* inMode)
{
   std::string pathStr = path.absolute() ? "/" : "";
   unsigned pathCreateDepth = excludeLastElement ? path.size() - 1 : path.size();

   mode_t mkdirMode = (inMode) ? *inMode : STORAGETK_DEFAULT_DIRMODE;

   int mkdirErrno = 0;

   for(size_t i=0; i < pathCreateDepth; i++)
   {
      pathStr += path[i];

      int mkRes = mkdir(pathStr.c_str(), mkdirMode);
      mkdirErrno = errno;
      if(mkRes && (mkdirErrno == EEXIST) )
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

      // prepare next loop
      pathStr += "/";
   }

   if (mkdirErrno == EEXIST)
      chmod(pathStr.c_str(), mkdirMode); // ensure the right path permissions for the last element


   return true;
}

/**
 * Create path directories, relative to a file descriptor
 *
 * Note: Existing directories are ignored (i.e. not an error).
 *
 * @param fd file descriptor of the "start directory"
 * @param relativePath path to create (inside fd)
 * @param excluseLastElement true if the last element should not be created.
 * @param inMode: NULL unless the caller sets it
 * @return true on success, false otherwise and errno is set from mkdir() in this case.
 */
bool StorageTk::createPathOnDisk(int fd, Path& relativePath, bool excludeLastElement,
   mode_t* inMode)
{
   std::string pathStr = relativePath.absolute() ? "/" : "";
   unsigned pathCreateDepth = excludeLastElement ? relativePath.size() -1 : relativePath.size();

   mode_t mkdirMode = (inMode) ? *inMode : STORAGETK_DEFAULT_DIRMODE;

   for ( unsigned i = 0; i < pathCreateDepth; i++ )
   {
      pathStr += relativePath[i];

      int mkRes = mkdirat(fd, pathStr.c_str(), mkdirMode);
      if ( mkRes != 0 )
      {
         if ( errno == EEXIST )
         { // path exists already => check whether it is a directory
            int pathFD = openat(fd, pathStr.c_str(), O_RDONLY);
            if ( pathFD == -1 ) // something went terribly wrong here
               return false;

            struct stat statStruct;
            int statRes = fstat(pathFD, &statStruct);

            if ( statRes || !S_ISDIR(statStruct.st_mode))
            {
               close(pathFD);
               return false;
            }

            // it is a existing directory; ensure the right path permissions
            fchmod(pathFD, mkdirMode);

            close(pathFD);
         }
         else
            return false;
      }

      // prepare next loop
      pathStr += "/";
   }

   return true;
}

/**
 * @param keepDepth the 1-based depth of the path that should not be deleted
 */
bool StorageTk::removePathDirsFromDisk(Path& path, unsigned keepDepth)
{
   StringVector pathElems;
   pathElems.reserve(path.size());
   for (size_t i = 0; i < path.size(); i++)
      pathElems.push_back(path[i]);

   std::string pathStr = path.absolute() ? "/" : "";
   return removePathDirsRec(pathElems.begin(), pathStr, 1, pathElems.size(), keepDepth);
}

/**
 * @param currentDepth 1-based path depth
 */
bool StorageTk::removePathDirsRec(StringVectorIter dirNameIter, std::string pathStr,
   unsigned currentDepth, unsigned numPathElems, unsigned keepDepth)
{
   pathStr += *dirNameIter;

   if(currentDepth < numPathElems)
   { // we're not at the end yet
      if(!removePathDirsRec(++StringVectorIter(dirNameIter), pathStr+"/",
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
 * Note: For ext3/4 it makes a difference whether this process is run by root or not
 * (f_bavail/f_bfree), so we use geteuid() in here to also detect this.
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

   /* note: outSizeFree depends on whether this process is running with superuser privileges or not
      (e.g. on ext3/4 file systems) */

   *outSizeTotal = statBuf.f_blocks * statBuf.f_bsize;

   if ( ( (unsigned)statBuf.f_type) != ( (unsigned)BTRFS_SUPER_MAGIC) )
      *outSizeFree = (geteuid() ? statBuf.f_bavail : statBuf.f_bfree) * statBuf.f_bsize;
   else
   {
      // f_bfree includes raid redundancy blocks on btrfs
      *outSizeFree = statBuf.f_bavail * statBuf.f_bsize;
   }

   *outInodesTotal = statBuf.f_files;
   *outInodesFree = statBuf.f_ffree;

   return true;
}

/**
 * Retrieve free/used space info for a mounted file system based on a path to a (sub)directory of
 * the mount point.
 *
 * Note: For ext3/4 it makes a difference whether this process is run by root or not
 * (f_bavail/f_bfree), so we use geteuid() in here to also detect this.
 *
 * @return false on error (and errno is set in this case)
 */
bool StorageTk::statStoragePath(Path& path, bool excludeLastElement, int64_t* outSizeTotal,
   int64_t* outSizeFree, int64_t* outInodesTotal, int64_t* outInodesFree)
{
   std::string pathStr = path.absolute() ? "/" : "";
   unsigned pathCreateDepth = excludeLastElement ? path.size() - 1 : path.size();

   for(unsigned i=0; i < pathCreateDepth; i++)
   {
      pathStr += path[i] + "/";
   }

   return statStoragePath(pathStr, outSizeTotal, outSizeFree, outInodesTotal, outInodesFree);
}

/**
 * Read free space info from override file, it if exists.
 *
 * @param path does not include filename of override file.
 * @param outSizeFree size read from override file if it exists (value will be left untouched
 * otherwise).
 * @param outInodesFree free inode count from override file if it exists (value will be left
 * untouched otherwise).
 *
 * @return true if either outSizeFree or inodeSizeFree or both values were set, false otherwise
 */
bool StorageTk::statStoragePathOverride(std::string pathStr, int64_t* outSizeFree,
      int64_t* outInodesFree)
{
   Path storageDirPath(pathStr);
   Path spaceFilePath = storageDirPath / STORAGETK_FREESPACE_FILENAME;
   Path inodesFilePath = storageDirPath / STORAGETK_FREEINODES_FILENAME;

   bool valueChanged = false;

   StringList spaceList; // actually, each of the files would contain only a single line/value
   StringList inodesList;

   bool spaceFilePathExists = StorageTk::pathExists(spaceFilePath.str() );
   if(spaceFilePathExists)
   {
      ICommonConfig::loadStringListFile(spaceFilePath.str().c_str(), spaceList);
      if(!(spaceList.empty() || spaceList.begin()->empty() ) )
      {
         *outSizeFree = UnitTk::strHumanToInt64(spaceList.begin()->c_str() );
         valueChanged = true;
      }
   }

   bool inodesFilePathExists = StorageTk::pathExists(inodesFilePath.str() );
   if(inodesFilePathExists)
   {
      ICommonConfig::loadStringListFile(inodesFilePath.str().c_str(), inodesList);
      if(!(inodesList.empty() || inodesList.begin()->empty() ) )
      {
         *outInodesFree = UnitTk::strHumanToInt64(inodesList.begin()->c_str() );
         valueChanged = true;
      }
   }

   return valueChanged;
}

/**
 * Returns the parent path for the given path by stripping the last entry name.
 *
 * Note: we have this for convenience, because ::dirname() might modify the passed buffer.
 */
std::string StorageTk::getPathDirname(std::string path)
{
   char* tmpPath = strdup(path.c_str() );

   std::string dirnameRes = ::dirname(tmpPath);

   free(tmpPath);

   return dirnameRes;
}

/**
 * Returns the last entry of the given path by stripping the parent path.
 *
 * Note: we have this for convenience, because ::basename() might modify the passed buffer.
 */
std::string StorageTk::getPathBasename(std::string path)
{
   char* tmpPath = strdup(path.c_str() );

   std::string dirnameRes = ::basename(tmpPath);

   free(tmpPath);

   return dirnameRes;
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

   Path filePath = dirPath / STORAGETK_DIR_LOCK_FILENAME;
   std::string filePathStr = filePath.str();

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
   unlockAndDeletePIDFile(fd);
}

/**
 * Note: Works by non-exclusive creation of a pid file and then trying to lock it non-blocking via
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
   int fd = open(pathStr.c_str(), O_CREAT | O_WRONLY, STORAGETK_DEFAULT_FILEMODE);
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
      if(updateRes == false)
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
void StorageTk::unlockAndDeletePIDFile(int fd)
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
   Path filePath = dirPath / STORAGETK_FORMAT_FILENAME;
   std::string filePathStr = filePath.str();

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

   try
   {
      MapTk::saveStringMapToFile(filePathStr.c_str(), &formatFileMap);
   }
   catch(InvalidConfigException& e)
   {
      std::cerr << "Unable to save storage format file (" << e.what() << "); " <<
         "SysErr: " << System::getErrString() << std::endl;

      return false;
   }

   return true;
}

void StorageTk::loadStorageFormatFile(const std::string pathStr, int minVersion, int maxVersion,
      int& fileVersion, StringMap& outFormatProperties)
{
   Path dirPath(pathStr);
   Path filePath = dirPath / STORAGETK_FORMAT_FILENAME;
   std::string filePathStr = filePath.str();

   StringMap formatFileMap;

   MapTk::loadStringMapFromFile(filePathStr.c_str(), &formatFileMap);

   if(formatFileMap.empty() )
      throw InvalidConfigException(std::string("Storage format file is empty: ") + filePathStr);

   StringMapIter iter = formatFileMap.find(STORAGETK_FORMAT_KEY_VERSION);
   if(iter == formatFileMap.end() )
   {
      throw InvalidConfigException(
         std::string("Storage format file does not contain version info: ") + filePathStr);
   }

   fileVersion = StringTk::strToInt(iter->second.c_str() );

   if(fileVersion < minVersion || fileVersion > maxVersion)
   {
      throw InvalidConfigException("Incompatible storage format: "
            + StringTk::intToStr(fileVersion)
            + " (required min: " +  StringTk::intToStr(minVersion)  +
            " required max : " + StringTk::intToStr(maxVersion) + ")");
   }

   outFormatProperties.swap(formatFileMap);
}

/**
 * @param pathStr path to the main storage working directory (not including a filename)
 * @param outFormatProperties all key/value pairs that were stored in the format file (may be NULL).
 * @throws exception if format file was not valid (eg didn't exist or contained wrong version).
 */
void StorageTk::checkAndUpdateStorageFormatFile(const std::string pathStr, int minVersion,
   int currentVersion, StringMap* outFormatProperties, bool forceUpdate)
{
   StringMap formatFileMap;
   int fileVersion;

   loadStorageFormatFile(pathStr, minVersion, currentVersion, fileVersion, formatFileMap);

   if (fileVersion < currentVersion || forceUpdate)
   {  // update the version
      Path dirPath(pathStr);
      Path filePath = dirPath / STORAGETK_FORMAT_FILENAME;
      std::string filePathStr = filePath.str();
      std::string backupFilePathString = filePathStr + STORAGETK_BACKUP_EXT;

      int renameRes = ::rename(filePathStr.c_str(), backupFilePathString.c_str() );
      if (renameRes)
      {
         throw InvalidConfigException(
            std::string("Failed to create " STORAGETK_FORMAT_FILENAME " backup file: ") +
            System::getErrString(errno) + ".");
      }

      bool createSuccess = createStorageFormatFile(pathStr, currentVersion, &formatFileMap);
      if (!createSuccess)
      {
         int renameRes = ::rename(backupFilePathString.c_str(), filePathStr.c_str() );
         if (renameRes)
         {
            throw InvalidConfigException(
               std::string("Failed to rename back " STORAGETK_FORMAT_FILENAME " backup file: ") +
               System::getErrString(errno) + ".");
         }

         throw InvalidConfigException(
            std::string("Failed to update " STORAGETK_FORMAT_FILENAME "!") );
      }
      else
      {
         int unlinkRes = ::unlink(backupFilePathString.c_str() );
         if (unlinkRes)
         {
            throw InvalidConfigException(
               std::string("Failed to unlink: ") + backupFilePathString + ": " +
               System::getErrString(errno) + ".");
         }
      }
   }

   if(outFormatProperties)
      outFormatProperties->swap(formatFileMap);
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
   Path filePath = dirPath / STORAGETK_FORMAT_FILENAME;
   std::string filePathStr = filePath.str();

   return pathExists(filePathStr.c_str() );
}

/**
 * Note: This is intended to check whether a storage directory was cleanly shut down, i.e. the
 * sessions file was created. In case of a server crash, there will be no sessions file.
 *
 * @param pathStr path to the main storage working directory (not including a filename)
 * @return true if sessions file exists
 */
bool StorageTk::checkSessionFileExists(const std::string& pathStr)
{
   Path dirPath(pathStr);
   Path filePath = dirPath / STORAGETK_SESSIONS_BACKUP_FILE_NAME;
   std::string filePathStr = filePath.str();

   return pathExists(filePathStr.c_str() );
}


/**
 * @param pathStr path to the storage working directory (not including a filename)
 * @thow InvalidConfigException on ID mismatch (or I/O error)
 */
void StorageTk::checkOrCreateOrigNodeIDFile(const std::string pathStr, std::string currentNodeID)
   throw(InvalidConfigException)
{
   // make sure that nodeID hasn't changed since last time

   Path metaPath(pathStr);
   Path nodeIDPath = metaPath / STORAGETK_ORIGINALNODEID_FILENAME;

   std::string origNodeID;
   try
   {
      StringList nodeIDList; // actually, the file would contain only a single line

      ICommonConfig::loadStringListFile(nodeIDPath.str().c_str(), nodeIDList);
      if(!nodeIDList.empty() )
         origNodeID = *nodeIDList.begin();
   }
   catch(InvalidConfigException& e) {}

   if(!origNodeID.empty() )
   { // compare original to current
      if(origNodeID != currentNodeID)
      {
         std::ostringstream outStream;
         outStream << "NodeID has changed from '" << origNodeID << "' to '" << currentNodeID <<
            "'. " << "Shutting down... " << "(File: " << nodeIDPath.str() << ")";

         throw InvalidConfigException(outStream.str() );
      }
   }
   else
   { // no origNodeID file yet => create file (based on currentNodeID)

      int fd = open(nodeIDPath.str().c_str(), O_CREAT | O_TRUNC | O_RDWR,
         STORAGETK_DEFAULT_FILEMODE);

      if(fd == -1)
      { // error
         int errCode = errno;

         std::ostringstream outStream;
         outStream << "Unable to create file for original node ID: " << nodeIDPath.str()
            << "; " << "SysErr: " << System::getErrString(errCode);

         throw InvalidConfigException(outStream.str() );
      }

      std::string currentNodeIDFileStr = currentNodeID + "\n";
      ssize_t writeRes = write(fd, currentNodeIDFileStr.c_str(), currentNodeIDFileStr.length() );
      IGNORE_UNUSED_VARIABLE(writeRes);

      fsync(fd);

      close(fd);
   }

}

/**
 * Reads the old targetID for the given path.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
void StorageTk::readTargetIDFile(const std::string pathStr, std::string* outTargetID)
   throw(InvalidConfigException)
{
   // check if file exists already and read it, otherwise create it with new targetID

   Path storageDirPath(pathStr);
   Path targetIDPath = storageDirPath / STORAGETK_TARGETID_FILENAME;

   StringList targetIDList; // actually, the file would contain only a single line

   bool targetIDPathExists = StorageTk::pathExists(targetIDPath.str() );
   if(targetIDPathExists)
      ICommonConfig::loadStringListFile(targetIDPath.str().c_str(), targetIDList);

   std::string oldTargetID;

   if(!targetIDList.empty() )
      oldTargetID = *targetIDList.begin();

   if(oldTargetID.empty() )
      return;


   *outTargetID = oldTargetID;
}

/**
 * Reads the old targetID for the given path or generates a new one and stores it.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 * @param localNodeID is part of the new targetID (if it needs to be generated)
 */
void StorageTk::readOrCreateTargetIDFile(const std::string pathStr, const NumNodeID localNodeID,
   std::string* outTargetID)
{
   // check if file exists already and read it, otherwise create it with new targetID

   std::string oldTargetID;

   readTargetIDFile(pathStr, &oldTargetID);

   if(!oldTargetID.empty() )
   { // targetID is already defined => we're done
      *outTargetID = oldTargetID;

      return;
   }

   // no oldTargetID file yet => create file and generate new target ID

   Path storageDirPath(pathStr);
   Path targetIDPath = storageDirPath / STORAGETK_TARGETID_FILENAME;

   std::string newTargetID = StorageTk::generateFileID(localNodeID); /* note: yes, new target IDs
      are generated the same way as file IDs */

   int fd = open(targetIDPath.str().c_str(), O_CREAT | O_TRUNC | O_RDWR,
      STORAGETK_DEFAULT_FILEMODE);
   if(fd == -1)
   { // error
      std::ostringstream outStream;
      outStream << "Unable to create file for storage target ID: " << targetIDPath.str()
         << "; " << "SysErr: " << System::getErrString();

      throw InvalidConfigException(outStream.str() );
   }

   std::string newTargetIDFileStr = newTargetID + "\n";
   ssize_t writeRes = write(fd, newTargetIDFileStr.c_str(), newTargetIDFileStr.length() );
   if(writeRes != (ssize_t) newTargetIDFileStr.length() )
   { // error
      std::ostringstream outStream;
      outStream << "Unable to write targetID file: " << targetIDPath.str()
         << "; " << "SysErr: " << System::getErrString();

      close(fd);

      throw InvalidConfigException(outStream.str() );
   }

   fsync(fd);
   close(fd);

   *outTargetID = newTargetID;
}

/**
 * Reads the old targetNumID for the given path.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 * @outTargetNumID 0 if not set yet (e.g. no such file exists), loaded ID otherwise
 */
void StorageTk::readNumIDFile(const std::string pathStr, const std::string filename,
   NumNodeID* outNodeNumID)
{
   // check if file exists already and read it
   Path storageDirPath(pathStr);
   Path targetIDPath = storageDirPath / filename;

   StringList targetIDList; // actually, the file would contain only a single line

   bool targetIDPathExists = StorageTk::pathExists(targetIDPath.str() );
   if(targetIDPathExists)
      ICommonConfig::loadStringListFile(targetIDPath.str().c_str(), targetIDList);

   std::string oldNodeNumID;

   if(!targetIDList.empty() )
      oldNodeNumID = *targetIDList.begin();

   if(oldNodeNumID.empty() )
      *outNodeNumID = NumNodeID(); // no numID defined yet
   else
      *outNodeNumID = NumNodeID(StringTk::strToUInt(oldNodeNumID) ); // ID is already defined
}

void StorageTk::readNumIDFile(const std::string pathStr, const std::string filename,
   uint16_t* outTargetNumID)
{
   // check if file exists already and read it
   Path storageDirPath(pathStr);
   Path targetIDPath = storageDirPath / filename;

   StringList targetIDList; // actually, the file would contain only a single line

   bool targetIDPathExists = StorageTk::pathExists(targetIDPath.str() );
   if(targetIDPathExists)
      ICommonConfig::loadStringListFile(targetIDPath.str().c_str(), targetIDList);

   std::string oldTargetNumID;

   if(!targetIDList.empty() )
      oldTargetNumID = *targetIDList.begin();

   if(oldTargetNumID.empty() )
      *outTargetNumID = 0; // no targetNumID defined yet
   else
      *outTargetNumID = StringTk::strToUInt(oldTargetNumID); // ID is already defined
}

/**
 * Create file for numeric target ID and write given ID into it.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
void StorageTk::createNumIDFile(const std::string pathStr, const std::string filename,
   uint16_t targetID)
{
   Path storageDirPath(pathStr);
   Path targetIDPath = storageDirPath / filename;

   std::string targetIDPathTmp = targetIDPath.str() + STORAGETK_TMPFILE_EXT;

   std::string newTargetIDStr = StringTk::uintToStr(targetID);

   // create tmp file with new targetID

   int fd = open(targetIDPathTmp.c_str(), O_CREAT | O_TRUNC | O_RDWR, STORAGETK_DEFAULT_FILEMODE);
   if(fd == -1)
   { // error
      std::ostringstream outStream;
      outStream << "Unable to create file for storage target ID: " << targetIDPathTmp << "; " <<
         "SysErr: " << System::getErrString();

      throw InvalidConfigException(outStream.str() );
   }

   std::string newTargetIDFileStr = newTargetIDStr + "\n";
   ssize_t writeRes = write(fd, newTargetIDFileStr.c_str(), newTargetIDFileStr.length() );
   if(writeRes != (ssize_t)newTargetIDFileStr.length() )
   { // error
      std::ostringstream outStream;
      outStream << "Unable to write targetID file: " << targetIDPathTmp << "; " <<
         "SysErr: " << System::getErrString();

      close(fd);

      throw InvalidConfigException(outStream.str() );
   }

   fsync(fd);
   close(fd);

   // rename tmp file to its actual name

   int renameRes = rename(targetIDPathTmp.c_str(), targetIDPath.str().c_str() );
   if(renameRes == -1)
   { // rename failed
      throw InvalidConfigException("Unable to rename numeric ID file: " + targetIDPathTmp + ". " +
         "SysErr: " + System::getErrString() );
   }
}

/**
 * Calls system readdir() and skips entries "." and ".." and META_DIRENTRYID_SUB_STR ("#fSiDs#").
 *
 * @return same as system readdir().
 */
struct dirent* StorageTk::readdirFiltered(DIR* dirp)
{
      return readdirFilteredEx(dirp, true, true);
}

/**
 * Calls system readdir() and optionally skips certain entries.
 *
 * @param filterDots true if you want to filter "." and "..".
 * @param filterFSIDsDir true if you want to filter META_DIRENTRYID_SUB_STR ("#fSiDs#").
 * @return same as system readdir() except that this will never return the filtered entries.
 */
struct dirent* StorageTk::readdirFilteredEx(DIR* dirp, bool filterDots,
   bool filterFSIDsDir)
{
      // loop to skip filtered entries
      for( ; ; )
      {
         errno = 0; // recommended by posix (readdir(3p) )

         struct dirent* dirEntry = readdir(dirp);

         if(!dirEntry)
            return dirEntry;

         if( (!filterDots || strcmp(dirEntry->d_name, ".") ) &&
             (!filterDots || strcmp(dirEntry->d_name, "..") ) &&
             (!filterFSIDsDir || strcmp(dirEntry->d_name, META_DIRENTRYID_SUB_STR) ) )
         {
            return dirEntry;
         }
      }
}

/**
 * Read all directory entries into given string list.
 *
 * Warning: This list can be big when the dir has lots of entries, so use this carefully.
 *
 * @throw InvalidConfigException on error (e.g. path not exists)
 */
void StorageTk::readCompleteDir(const char* path, StringList* outNames)
   throw(InvalidConfigException)
{
   errno = 0; // recommended by posix (readdir(3p) )

   DIR* dirHandle = opendir(path);
   if(!dirHandle)
      throw InvalidConfigException(std::string("Unable to open directory: ") + path + "; "
         "SysErr: " + System::getErrString() );

   struct dirent* dirEntry;

   while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
   {
      outNames->push_back(dirEntry->d_name);
   }

   if(errno)
   {
      throw InvalidConfigException(
         std::string("Unable to fetch directory entry from: ") + path + "; "
         "SysErr: " + System::getErrString() );
   }

   closedir(dirHandle);
}



/**
 * Get path to chunk file and additional Path to chunk file (not including file name), both
 * relative to chunks subdir.
 *
 * If hasOrigFeature is set then the returned path is 2014.01-style layout with user and timestamp
 * dirs, otherwise it's the previous 2012.10 style layout with only hash dirs.
 *
 * @param hasOrigFeature true for 2014.01 style, false for 2012.10 style.
 * @param outChunkDirPathpath to parent dir of chunk file (filled-in for >V2 only).
 * @param outChunkFilePathStr path to chunk file.
 */
void StorageTk::getChunkDirChunkFilePath(const PathInfo* pathInfo, std::string entryID,
   bool hasOrigFeature, Path& outChunkDirPath, std::string& outChunkFilePathStr)
{
   if (!hasOrigFeature)
   {  // old  2012.10 style storage layout
      outChunkFilePathStr = StorageTk::getFileChunkPathV2(entryID, &outChunkDirPath);
   }
   else
   { // new 2014.01 style storage layout
      StorageTk::getChunkDirV3Path(pathInfo, outChunkDirPath);
      outChunkFilePathStr = outChunkDirPath.str() + '/' + entryID;
   }
}

bool StorageTk::removeDirRecursive(const std::string& dir)
{
   struct ops
   {
      static int visit(const char* path, const struct stat*, int type, struct FTW*)
      {
         if(type == FTW_F)
            return ::unlink(path);
         else
            return ::rmdir(path);
      }
   };

   return ::nftw(dir.c_str(), ops::visit, 10, FTW_DEPTH) == 0;
}
