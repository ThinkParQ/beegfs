#include <common/app/config/ICommonConfig.h>
#include <common/toolkit/MapTk.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/TimeAbs.h>
#include "StorageTk.h"

#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/statfs.h>
#include <aio.h>

#define STORAGETK_DIR_LOCK_FILENAME          "lock.pid"

#define STORAGETK_FORMAT_FILENAME            "format.conf"
#define STORAGETK_FORMAT_KEY_VERSION         "version"

#define STORAGETK_TMPFILE_EXT                ".tmp" /* tmp extension for saved files until rename */

#define STORAGETK_DEFAULT_FILEMODE           (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)


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
            throw InvalidConfigException("Unable to create subdir: " +
               level1SubDirPath.getPathAsStr() );

         for (int level2=0; level2 < maxLevel2; level2++)
         {
            std::string level2SubDirName = StringTk::uintToHexStr(level2);
            Path level2SubDirPath(level1SubDirPath, level2SubDirName);

            if(!StorageTk::createPathOnDisk(level2SubDirPath, false) )
               throw InvalidConfigException("Unable to create subdir: " +
                  level2SubDirPath.getPathAsStr() );
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
 * @return true on success, false otherwise and errno is set from mkdir() in this case.
 */
bool StorageTk::createPathOnDisk(Path& path, bool excludeLastElement)
{
   const StringList* pathElems = path.getPathElems();
   std::string pathStr = path.isAbsolute() ? "/" : "";
   unsigned pathCreateDepth = excludeLastElement ? pathElems->size() -1 : pathElems->size();

   StringListConstIter iter = pathElems->begin();
   for(unsigned i=0; i < pathCreateDepth; iter++, i++)
   {
      pathStr += *iter;

      int mkRes = mkdir(pathStr.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
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

/*
 * remove all files(!!) inside a directory
 */
bool StorageTk::removePathDirContentsFromDisk(Path& path)
{
   bool retVal = true;

   std::string pathStr = path.getPathAsStr();

   DIR *directory;
   struct dirent *entry;

   directory = opendir(pathStr.c_str());
   do
   {
       entry = StorageTk::readdirFiltered(directory);
       if (entry)
       {
          std::string entryName = pathStr + "/" + entry->d_name;
          int unlinkRes = unlink(entryName.c_str());
          if (unlinkRes != 0)
          {
             retVal = false;
             goto closeDirectory;
          }

       }
   } while (entry);

   closeDirectory:
   closedir(directory);

   return retVal;
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
   *outSizeFree = (geteuid() ? statBuf.f_bavail : statBuf.f_bfree) * statBuf.f_bsize;
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
   const StringList* pathElems = path.getPathElems();
   std::string pathStr = path.isAbsolute() ? "/" : "";
   unsigned pathCreateDepth = excludeLastElement ? pathElems->size() -1 : pathElems->size();

   StringListConstIter iter = pathElems->begin();
   for(unsigned i=0; i < pathCreateDepth; iter++, i++)
   {
      pathStr += *iter + "/";
   }

   return statStoragePath(pathStr, outSizeTotal, outSizeFree, outInodesTotal, outInodesFree);
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
#if 0 // for the upgrade tool we overwrite this file with the new version
   if(pathExists(filePathStr.c_str() ) )
      return true;
#endif

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

/**
 * @param pathStr path to the main storage working directory (not including a filename)
 * @param outFormatProperties all key/value pairs that were stored in the format file (may be NULL).
 * @throws exception if format file was not valid (eg didn't exist or contained wrong version).
 */
bool StorageTk::checkStorageFormatFile(const std::string pathStr, int oldFormatVersion,
   int newFormatVersion, StringMap* outFormatProperties) throw(InvalidConfigException)
{
   Path dirPath(pathStr);
   Path filePath(dirPath, STORAGETK_FORMAT_FILENAME);
   std::string filePathStr = filePath.getPathAsStr();

   StringMap formatFileMap;

   MapTk::loadStringMapFromFile(filePathStr.c_str(), &formatFileMap);

   if(formatFileMap.empty() )
      throw InvalidConfigException(std::string("Storage format file is empty: ") + filePathStr);

   StringMapIter iter = formatFileMap.find(STORAGETK_FORMAT_KEY_VERSION);
   if(iter == formatFileMap.end() )
   {
      throw InvalidConfigException(
         std::string("Storage format file does not contain version info: ") + filePathStr);

      return false;
   }

   int formatVersion = StringTk::strToInt(iter->second.c_str() );
   if(formatVersion != oldFormatVersion && formatVersion != newFormatVersion)
   { // we allow the new version, as the upgrade tool might need to be run several times
     // (possible error) and the directories differ anyway
         throw InvalidConfigException(std::string("Incompatible storage format: ") +
            iter->second + " (required: " + StringTk::intToStr(oldFormatVersion) + ")");
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
 * @param pathStr path to the storage working directory (not including a filename)
 * @thow InvalidConfigException on ID mismatch (or I/O error)
 */
void StorageTk::checkOrCreateOrigNodeIDFile(const std::string pathStr, std::string currentNodeID)
   throw(InvalidConfigException)
{
   // make sure that nodeID hasn't changed since last time

   Path metaPath(pathStr);
   Path nodeIDPath(metaPath, STORAGETK_ORIGINALNODEID_FILENAME);

   std::string origNodeID;
   try
   {
      StringList nodeIDList; // actually, the file would contain only a single line

      ICommonConfig::loadStringListFile(nodeIDPath.getPathAsStr().c_str(), nodeIDList);
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
            "'. " << "Shutting down... " << "(File: " << nodeIDPath.getPathAsStr() << ")";

         throw InvalidConfigException(outStream.str() );
      }
   }
   else
   { // no origNodeID file yet => create file (based on currentNodeID)

      int fd = open(nodeIDPath.getPathAsStr().c_str(), O_CREAT | O_TRUNC | O_RDWR,
         STORAGETK_DEFAULT_FILEMODE);

      if(fd == -1)
      { // error
         int errCode = errno;

         std::ostringstream outStream;
         outStream << "Unable to create file for original node ID: " << nodeIDPath.getPathAsStr()
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
   Path targetIDPath(storageDirPath, STORAGETK_TARGETID_FILENAME);

   StringList targetIDList; // actually, the file would contain only a single line

   bool targetIDPathExists = StorageTk::pathExists(targetIDPath.getPathAsStr() );
   if(targetIDPathExists)
      ICommonConfig::loadStringListFile(targetIDPath.getPathAsStr().c_str(), targetIDList);

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
void StorageTk::readOrCreateTargetIDFile(const std::string pathStr, uint16_t localNodeID,
   std::string* outTargetID) throw(InvalidConfigException)
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
   Path targetIDPath(storageDirPath, STORAGETK_TARGETID_FILENAME);

   std::string newTargetID = StorageTk::generateFileID(localNodeID); /* note: yes, new target IDs
      are generated the same way as file IDs */

   int fd = open(targetIDPath.getPathAsStr().c_str(), O_CREAT | O_TRUNC | O_RDWR,
      STORAGETK_DEFAULT_FILEMODE);
   if(fd == -1)
   { // error
      std::ostringstream outStream;
      outStream << "Unable to create file for storage target ID: " << targetIDPath.getPathAsStr()
         << "; " << "SysErr: " << System::getErrString();

      throw InvalidConfigException(outStream.str() );
   }

   std::string newTargetIDFileStr = newTargetID + "\n";
   ssize_t writeRes = write(fd, newTargetIDFileStr.c_str(), newTargetIDFileStr.length() );
   if(writeRes != (ssize_t) newTargetIDFileStr.length() )
   { // error
      std::ostringstream outStream;
      outStream << "Unable to write targetID file: " << targetIDPath.getPathAsStr()
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
   uint16_t* outTargetNumID) throw(InvalidConfigException)
{
   // check if file exists already and read it

   Path storageDirPath(pathStr);
   Path targetIDPath(storageDirPath, filename);

   StringList targetIDList; // actually, the file would contain only a single line

   bool targetIDPathExists = StorageTk::pathExists(targetIDPath.getPathAsStr() );
   if(targetIDPathExists)
      ICommonConfig::loadStringListFile(targetIDPath.getPathAsStr().c_str(), targetIDList);

   std::string oldTargetNumID;

   if(!targetIDList.empty() )
      oldTargetNumID = *targetIDList.begin();

   if(oldTargetNumID.empty() )
      *outTargetNumID = 0; // no targetNumID defined yet
   else
      *outTargetNumID = StringTk::strToUInt(oldTargetNumID); // targetID is already defined
}

/**
 * Create file for numeric target ID and write given ID into it.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
void StorageTk::createNumIDFile(const std::string pathStr, const std::string filename,
   uint16_t targetID) throw(InvalidConfigException)
{
   Path storageDirPath(pathStr);
   Path targetIDPath(storageDirPath, filename);

   std::string targetIDPathTmp = targetIDPath.getPathAsStr() + STORAGETK_TMPFILE_EXT;

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

   int renameRes = rename(targetIDPathTmp.c_str(), targetIDPath.getPathAsStr().c_str() );
   if(renameRes == -1)
   { // rename failed
      throw InvalidConfigException("Unable to rename numeric ID file: " + targetIDPathTmp + ". " +
         "SysErr: " + System::getErrString() );
   }
}

/**
 * Calls system readdir() and skips entries "." and ".."
 *
 * @return same as system readdir() except that this will never return the entries ".", ".." and
 * META_DIRENTRYID_SUB_STR ("#fSiDs#")
 */
struct dirent* StorageTk::readdirFiltered(DIR* dirp)
{
      errno = 0; // recommended by posix (readdir(3p) )

      /* TODO: strcmp() is rather expensive. We could check if we already filtered n-entries
       *       (currently 3) and skip further string compares then. Assuming tobe-filtered entries
       *       are the first n-entries read, this should a bit faster. As we cannot rely on that
       *       the number of already-filtered entries has to be provided as client cookie.
       */

      for( ; ; )
      {
         struct dirent* dirEntry = readdir(dirp);
         if(!dirEntry)
         {
            return dirEntry;
         }
         else
         if(strcmp(dirEntry->d_name, ".") &&
            strcmp(dirEntry->d_name, "..")  &&
            strcmp(dirEntry->d_name, META_DIRENTRYID_SUB_STR) )
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
 * Make IO using aio_... methods and wait for completion.
 *
 * Warning: After this request, the value of the current file position is unspecified.
 * Note: Parameters and return value similar to pread().
 */
ssize_t StorageTk::readAIOSync(int fd, void* buf, size_t count, int64_t offset)
{
   struct aiocb aioControl;
   struct aiocb* aioControlList[] = { &aioControl };

   memset(&aioControl, 0, sizeof(aioControl) );

   aioControl.aio_fildes = fd;
   aioControl.aio_buf = buf;
   aioControl.aio_nbytes = count;
   aioControl.aio_offset = offset;

   int readStartRes = aio_read(&aioControl);
   if(unlikely(readStartRes == -1) )
      return -1;

   int errorRes = aio_error(&aioControl); // errorRes becomes our internal errno from here on
   if(errorRes == EINPROGRESS)
   { // normal case => wait for completion

      /* note: we have a loop around aio_suspend because it might be interrupted for various
         reasons (eg signal) and we definitely need our io to be completed before we return. */
      do
      {
         aio_suspend(aioControlList, 1, NULL);
         errorRes = aio_error(&aioControl);

      } while(unlikely(errorRes == EINPROGRESS) );
   }

   if(unlikely(errorRes) )
   { // aio_error doesn't set errno, so we do it to make life easier for the caller.
      errno = errorRes;
   }

   ssize_t finalRes = aio_return(&aioControl); // must always be called to clean up stuff

   return finalRes;
}

/**
 * Make IO using aio_... methods and wait for completion.
 *
 * Warning: After this request, the value of the current file position is unspecified.
 * Note: Parameters and return value similar to pwrite().
 */
ssize_t StorageTk::writeAIOSync(int fd, void* buf, size_t count, int64_t offset)
{
   struct aiocb aioControl;
   struct aiocb* aioControlList[] = { &aioControl };

   memset(&aioControl, 0, sizeof(aioControl) );

   aioControl.aio_fildes = fd;
   aioControl.aio_buf = buf;
   aioControl.aio_nbytes = count;
   aioControl.aio_offset = offset;

   int readStartRes = aio_write(&aioControl);
   if(unlikely(readStartRes == -1) )
      return -1;

   int errorRes = aio_error(&aioControl); // errorRes becomes our internal errno from here on
   if(errorRes == EINPROGRESS)
   { // normal case => wait for completion

      /* note: we have a loop around aio_suspend because it might be interrupted for various
         reasons (eg signal) and we definitely need our io to be completed before we return. */
      do
      {
         aio_suspend(aioControlList, 1, NULL);
         errorRes = aio_error(&aioControl);

      } while(unlikely(errorRes == EINPROGRESS) );
   }

   if(unlikely(errorRes) )
   { // aio_error doesn't set errno, so we do it to make life easier for the caller.
      errno = errorRes;
   }

   ssize_t finalRes = aio_return(&aioControl); // must always be called to clean up stuff

   return finalRes;
}

/**
 * Note: This version is compatible with sparse files.
 */
void StorageTk::updateDynamicFileInodeAttribs(StripeNodeFileInfoVec& fileInfoVec,
   StripePattern* stripePattern, StatData& outStatData)
{
   // we use the dynamic attribs only if they have been initialized (=> storageVersion != 0)

   // dynamic attrib algo:
   // walk over all the stripeNodes and...
   // - find last node with max number of (possibly incomplete) chunks
   // - find the latest modification- and lastAccessTimeSecs


   // scan for an initialized index...
   /* note: we assume that in most cases, the user will stat not-opened files (which have
      all storageVersions set to 0), so we optimize for that case. */

   int firstValidDynAttribIndex = -1; // -1 means "no valid index found"

   for(size_t i=0; i < fileInfoVec.size(); i++)
   {
      if(fileInfoVec[i].getStorageVersion() )
      {
         firstValidDynAttribIndex = i;
         break;
      }
   }

   if(firstValidDynAttribIndex == -1)
   { // no valid dyn attrib element found => use static attribs and nothing to do
      return;
   }

   /* we found a valid dyn attrib index => start with first target anyways to avoid false file
      length computations... */

   unsigned lastMaxNumChunksIndex = 0;
   int64_t maxNumChunks = fileInfoVec[0].getNumChunks();

   int64_t newModificationTimeSecs = fileInfoVec[0].getModificationTimeSecs();
   int64_t newLastAccessTimeSecs = fileInfoVec[0].getLastAccessTimeSecs();
   int64_t newFileSize;

   for(unsigned i = 1; i < fileInfoVec.size(); i++)
   {
      /* note: we cannot ignore storageVersion==0 here, because that would lead to wrong file
       *       length computations.
       * note2: fileInfoVec will be initialized once we have read meta data from disk, so if a
       *        storage target does not answer or no request was sent to it,
       *        we can still use old data from fileInfoVec
       *
       * */

      int64_t currentNumChunks = fileInfoVec[i].getNumChunks();

      if(currentNumChunks >= maxNumChunks)
      {
         lastMaxNumChunksIndex = i;
         maxNumChunks = currentNumChunks;
      }

      int64_t currentModificationTimeSecs = fileInfoVec[i].getModificationTimeSecs();
      if(currentModificationTimeSecs > newModificationTimeSecs)
         newModificationTimeSecs = currentModificationTimeSecs;

      int64_t currentLastAccessTimeSecs = fileInfoVec[i].getLastAccessTimeSecs();
      if(currentLastAccessTimeSecs > newLastAccessTimeSecs)
         newLastAccessTimeSecs = currentLastAccessTimeSecs;
   }

   if(maxNumChunks)
   { // we have chunks, so "fileLength > 0"

      // note: we do all this complex filesize calculation stuff here to support sparse files

      // every target before lastMax-target must have the same number of (complete) chunks
      int64_t lengthBeforeLastMaxIndex = lastMaxNumChunksIndex * maxNumChunks *
         stripePattern->getChunkSize();

      // every target after lastMax-target must have exactly one chunk less than the lastMax-target
      int64_t lengthAfterLastMaxIndex = (fileInfoVec.size() - lastMaxNumChunksIndex - 1) *
         (maxNumChunks - 1) * stripePattern->getChunkSize();

      // all the prior calcs are based on the lastMax-target size, so we can simply use that one
      int64_t lengthLastMaxNode = fileInfoVec[lastMaxNumChunksIndex].getLength();

      int64_t totalLength = lengthBeforeLastMaxIndex + lengthAfterLastMaxIndex + lengthLastMaxNode;

      newFileSize = totalLength;
   }
   else
   { // no chunks, no filesize
      newFileSize = 0;
   }


   // now update class values
   outStatData.setModificationTimeSecs(newModificationTimeSecs);
   outStatData.setLastAccessTimeSecs(newLastAccessTimeSecs);
   outStatData.setFileSize(newFileSize);
}
