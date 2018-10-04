#include <common/app/config/ICommonConfig.h>
#include <common/storage/Metadata.h>
#include <common/toolkit/FDHandle.h>
#include <common/toolkit/MapTk.h>
#include <common/toolkit/TempFileTk.h>
#include <common/toolkit/TimeAbs.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/UnitTk.h>
#include "StorageTk.h"

#include <ftw.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/statfs.h>
#include <libgen.h>

#define STORAGETK_DIR_LOCK_FILENAME          "lock.pid"

#define STORAGETK_FORMAT_KEY_VERSION         "version"

#define STORAGETK_TMPFILE_EXT                ".tmp" /* tmp extension for saved files until rename */

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
 * Note: The PID inside the file might no longer be up-to-date if this is called before a daemon
 * forks to background mode, so don't rely on it.
 *
 * @param pathStr path to a directory (not including a filename)
 * @return invalid on error, valid on success
 */
LockFD StorageTk::lockWorkingDirectory(const std::string& pathStr)
{
   Path dirPath(pathStr);
   bool createPathRes = StorageTk::createPathOnDisk(dirPath, false);
   if(!createPathRes)
   {
      int errCode = errno;
      std::cerr << "Unable to create working directory: " << pathStr << "; " <<
         "SysErr: " << System::getErrString(errCode) << std::endl;

      return {};
   }

   Path filePath = dirPath / STORAGETK_DIR_LOCK_FILENAME;
   std::string filePathStr = filePath.str();

   LockFD fd = createAndLockPIDFile(filePathStr, false);
   if (!fd.valid())
   {
      std::cerr << "Unable to lock working directory. Check if the directory is already in use: " <<
         pathStr << std::endl;
   }

   return fd;
}

/**
 * Note: Works by non-exclusive creation of a pid file and then trying to lock it non-blocking via
 * flock(2).
 *
 * @param pathStr path (including a filename) of the pid file, parent dirs will not be created
 * @param writePIDToFile whether or not you want to write the PID to the file (if false, the file
 * will just be truncated to 0 size); false is useful before a daemon forked to background mode.
 * @return invalid on error, valid on success
 */
LockFD StorageTk::createAndLockPIDFile(const std::string& pathStr, bool writePIDToFile)
{
   return LockFD::lock(pathStr, writePIDToFile)
      .reduce(
            [&] (LockFD lock) -> LockFD {
               const auto updateErr = lock.updateWithPID();
               if (writePIDToFile && updateErr)
               {
                  std::cerr << "Problem encountered during PID file update: " << updateErr.message()
                     << std::endl;
                  return {};
               }

               return lock;
            },
            [&] (const std::error_code& error) -> LockFD {
               if (error.value() == EWOULDBLOCK)
                  std::cerr << "Unable to lock PID file: " << pathStr << " (already locked)."
                     << std::endl << "Check if service is already running." << std::endl;
               else
                  std::cerr << "Unable to lock working directory lock file: " << pathStr
                     << "; " << "SysErr: " << error.message() << std::endl;

               return {};
            });
}

bool StorageTk::writeFormatFile(const std::string& path, int formatVersion,
   const StringMap* formatProperties)
{
   Path dirPath(path);
   Path filePath = dirPath / STORAGETK_FORMAT_FILENAME;

   std::stringstream contents;

   contents << "# This file was auto-generated. Do not modify it!" << std::endl;
   contents << STORAGETK_FORMAT_KEY_VERSION << "=" << formatVersion << std::endl;

   if (formatProperties)
   {
      for (auto it = formatProperties->begin(); it != formatProperties->end(); ++it)
      {
         if (it->first != STORAGETK_FORMAT_KEY_VERSION)
            contents << it->first << "=" << it->second << std::endl;
      }
   }

   const auto contentsStr = contents.str();

   const auto saveRes = TempFileTk::storeTmpAndMove(filePath.str(), contentsStr);
   return saveRes == FhgfsOpsErr_SUCCESS;
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

   return writeFormatFile(pathStr, formatVersion, formatProperties);
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
 * @throws exception if format file was not valid (eg didn't exist or contained wrong version).
 * @return all key/value pairs that were stored in the format file
 */
StringMap StorageTk::loadAndUpdateStorageFormatFile(const std::string pathStr, int minVersion,
   int currentVersion)
{
   StringMap formatFileMap;
   int fileVersion;

   loadStorageFormatFile(pathStr, minVersion, currentVersion, fileVersion, formatFileMap);

   if (fileVersion < currentVersion)
   {
      if (!writeFormatFile(pathStr, currentVersion, &formatFileMap))
         throw InvalidConfigException("Failed to update " STORAGETK_FORMAT_FILENAME "!");
   }

   return formatFileMap;
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
      const std::string fileContents = 
            currentNodeID + "\n"
            "# This file was auto-generated and must not be modified. If your hostname has "
            "changed, create a\n# copy of this file under the name \"nodeID\", keep the content "
            "unchanged and restart this service.";
            
      const auto writeRes = TempFileTk::storeTmpAndMove(nodeIDPath.str(), fileContents + "\n");
      if (writeRes != FhgfsOpsErr_SUCCESS)
         throw InvalidConfigException("Unable to create file for storage target ID "
               + nodeIDPath.str());
   }
}

/**
 * Reads the old targetID for the given path.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
void StorageTk::readTargetIDFile(const std::string pathStr, std::string* outTargetID)
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

   // note: yes, new target IDs are generated the same way as file IDs
   const auto newTargetID = StorageTk::generateFileID(localNodeID);

   const auto writeRes = TempFileTk::storeTmpAndMove(targetIDPath.str(), newTargetID + "\n");
   if (writeRes != FhgfsOpsErr_SUCCESS)
      throw InvalidConfigException("Unable to create file for storage target ID "
            + targetIDPath.str());

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
   auto res = readNumFromFile<NumNodeID>(pathStr, filename);
   if (res.first == FhgfsOpsErr_SUCCESS)
      *outNodeNumID = res.second;
   else
      *outNodeNumID = NumNodeID(0);
}

/*
 * @return the numeric value contained in the targetIDFile or 0 if file doesn't exist or error
 *         occured
 */
void StorageTk::readNumTargetIDFile(const std::string pathStr, const std::string filename,
   uint16_t* outTargetNumID)
{
   auto res = readNumFromFile<uint16_t>(pathStr, filename);
   if (res.first == FhgfsOpsErr_SUCCESS)
      *outTargetNumID = res.second;
   else
      *outTargetNumID = 0;
}

StoragePoolId StorageTk::readNumStoragePoolIDFile(const std::string pathStr,
   const std::string filename)
{
   auto res = readNumFromFile<StoragePoolId>(pathStr, filename);
   if (res.first == FhgfsOpsErr_SUCCESS)
      return res.second;
   else
      return StoragePoolStore::DEFAULT_POOL_ID;
}

/**
 * Create file for numeric target ID and write given ID into it.
 *
 * @param pathStr path to the storage working directory (not including a filename)
 */
void StorageTk::createNumIDFile(const std::string pathStr, const std::string filename,
   uint16_t targetID)
{
   // TODO: cast to unsigned long is a workaround for a bug in gcc 4.4
   const auto targetIDStr = std::to_string((unsigned long long) targetID) + "\n";

   Path storageDirPath(pathStr);
   Path targetIDPath = storageDirPath / filename;
  //check if the file already exists, create file only if it does not exists
   auto res = readNumFromFile<uint16_t>(pathStr, filename);
   if ( res.first == FhgfsOpsErr_PATHNOTEXISTS ) 
   {
      const auto writeRes = TempFileTk::storeTmpAndMove(targetIDPath.str(), targetIDStr);
      if (writeRes != FhgfsOpsErr_SUCCESS)
         throw InvalidConfigException("Unable to create file for storage target ID "
         + std::to_string((unsigned long long) targetID));
   }
   else if ( res.second != targetID )
   {
         throw InvalidConfigException("storage target ID  mismatch");
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

         if( (!filterDots || strcmp(dirEntry->d_name, ".") != 0 ) &&
             (!filterDots || strcmp(dirEntry->d_name, "..") != 0 ) &&
             (!filterFSIDsDir || strcmp(dirEntry->d_name, META_DIRENTRYID_SUB_STR) != 0 ) )
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

/**
 * Finds the longest prefix of path that is a mountpoint. path must be an absolute pathname, but
 * it need not exist. Symlink resolution is not performed on path.
 *
 * @param rawPath path to resolve to its longest-prefix mountpoint
 * @param mtab a std::istream with contents in the format of /etc/mtab or /proc/mounts
 */
std::pair<bool, Mount> StorageTk::findLongestMountedPrefix(const std::string& rawPath,
      std::istream& mtab)
{
   if (rawPath.empty() || rawPath[0] != '/')
      return {false, {}};

   Mount currentMount = {};

   // note: this code parses the mtab input directly instead of calling getmntent because getmnt
   // is not reentrant and getmntent_r is limited by the buffer passed to it (without giving useful
   // indications that the buffer is too small).
   // we will also want to switch to the mountinfo format (of /proc/self/mountinfo) at some point
   // in the future, which is not supported by getmntent at all, but parses much the same as here.
   while (mtab) {
      std::string device, mount, type;
      std::string::size_type pos;

      // mtab format files have these fields:
      //  <devname> SP <mountpoint> SP <other fields> NL
      // devname and mountpoint escape the [ \t\n\\] characters with octal sequences
      getline(mtab, device, ' ');
      getline(mtab, mount, ' ');
      getline(mtab, type, ' ');
      if (!mtab)
         break;
      while ((pos = mount.find("\\040")) != std::string::npos)
         mount.replace(pos, 4, " ");
      while ((pos = mount.find("\\011")) != std::string::npos)
         mount.replace(pos, 4, "\t");
      while ((pos = mount.find("\\012")) != std::string::npos)
         mount.replace(pos, 4, "\n");
      while ((pos = mount.find("\\134")) != std::string::npos)
         mount.replace(pos, 4, "\\");

      mtab.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

      if (mount == rawPath)
         return {true, Mount{std::move(device), std::move(mount), std::move(type)}};
      if (rawPath.size() <= mount.size())
         continue;
      if (rawPath.compare(0, mount.size(), mount) != 0)
         continue;
      if (*mount.rbegin() != '/' && rawPath[mount.size()] != '/')
         continue;
      if (mount.size() < currentMount.path.size())
         continue;

      currentMount = {std::move(device), std::move(mount), std::move(type)};
   }

   if (mtab.eof() && !currentMount.path.empty())
      return {true, std::move(currentMount)};
   else
      return {false, {}};
}

/**
 * Finds the longest prefix of path that is a mountpoint. path must be an absolute pathname, but
 * it need not exist. Symlink resolution is performed on path up to an including the last component.
 * Mountpoints are looked up from /proc/self/mounts.
 *
 * @param path path to resolve to its longest-prefix mountpoint
 */
std::pair<bool, Mount> StorageTk::findMountForPath(const std::string& path)
{
   std::ifstream mounts("/proc/self/mounts");

   if (!mounts)
      return {false, {}};

   std::string resolvedPath;

   {
      char* resolved = ::realpath(path.c_str(), nullptr);
      if (!resolved)
         return {false, {}};

      resolvedPath = resolved;
      free(resolved);
   }

   return findLongestMountedPrefix(resolvedPath, mounts);
}

/**
 * Reads a file into a buffer, up to the size specified.
 * @param path The path to be read
 * @param maxSize The maximum number of bytes to be read
 * @returns Pair of FhfgfsOpsError and a char vector.
 *          In case of error, empty vector is returned and Error code is set as follows:
 *          FhgfsOpsErr_RANGE - File is larger than maxSize
 *          Appropriate error (via fromSysErr) if open() fails
 *          Otherwise, the error code is set to FhgfsOpsErr_SUCCESS and vector is filled
 *          with file contents.
 */
std::pair<FhgfsOpsErr, std::vector<char>> StorageTk::readFile(const std::string& filename,
      int maxSize)
{
   FDHandle fd(::open(filename.c_str(), O_RDONLY));
   if (!fd.valid())
      return { FhgfsOpsErrTk::fromSysErr(errno), {} };

   auto size = ::lseek(fd.get(), 0, SEEK_END);
   if (size > maxSize)
      return { FhgfsOpsErr_RANGE, {} };

   if (size == -1)
      return { FhgfsOpsErrTk::fromSysErr(errno), {} };

   auto seekRes = ::lseek(fd.get(), 0, SEEK_SET);
   if (seekRes == -1)
      return { FhgfsOpsErrTk::fromSysErr(errno), {} };

   std::vector<char> buf(size);
   auto readRes = ::read(fd.get(), &buf[0], size);

   if (readRes == -1)
      return { FhgfsOpsErrTk::fromSysErr(errno), {} };

   return { FhgfsOpsErr_SUCCESS, buf };
}

/*
 * reads a numeric value from a file (Note: the file must only contain the numeric value)
 *
 * @param pathStr dirname to look into
 * @filename the file to read
 * @outLines the file contents as list (one element per line)
 *
 * @return a air of FhfgfsOpsErr and the numeric value.
 *         In case of error, 0 is returned and Error code is set as follows:
 *         FhgfsOpsErr_PATHNOTEXISTS if file doesn't
 *         FhgfsOpsErr_INVAL if the cast from string to num failed
 *         FhgfsOpsErr_NODATA if file exists, but is empty
 */
template<typename NumericT>
std::pair<FhgfsOpsErr,NumericT> StorageTk::readNumFromFile(const std::string& pathStr,
      const std::string& filename)
{
   // check if file exists and read it
   Path dirPath(pathStr);
   Path filePath = dirPath / filename;

   bool fileExists = StorageTk::pathExists(filePath.str() );
   if (!fileExists)
      return { FhgfsOpsErr_PATHNOTEXISTS, NumericT(0) };

   // read the file
   StringList contents;
   ICommonConfig::loadStringListFile(filePath.str().c_str(), contents);

   if ( contents.empty() )
      return { FhgfsOpsErr_NODATA, NumericT(0) };

   // we are only interested in the first element of contents, as the file should only contain
   // one numeric value
   std::stringstream ss(contents.front());
   NumericT value;

   ss >> value;

   if (!ss.fail())
      return { FhgfsOpsErr_SUCCESS, value };
   else
      return { FhgfsOpsErr_INVAL, NumericT(0) };
}
