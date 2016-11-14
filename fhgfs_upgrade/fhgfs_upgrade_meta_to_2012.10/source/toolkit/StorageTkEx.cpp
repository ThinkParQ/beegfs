#include <program/Program.h>
#include "StorageTkEx.h"

#include <attr/xattr.h>

#define STORAGETK_FORMAT_XATTR   "xattr"


/**
 * Note: Creates the file only if it does not exist yet.
 *
 * @return true if format file was created or existed already
 */
bool StorageTkEx::createStorageFormatFile(const std::string pathStr)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   StringMap formatProperties;

   formatProperties[STORAGETK_FORMAT_XATTR] = cfg->getStoreUseExtendedAttribs() ? "true" : "false";

   return StorageTk::createStorageFormatFile(pathStr, STORAGETK_FORMAT_CURRENT_VERSION,
      &formatProperties);
}

/**
 * Compatibility and validity check of storage format file contents.
 *
 * @param pathStr path to the main storage working directory (not including a filename)
 * @throws exception if format file was not valid (eg didn't exist or contained wrong version).
 */
bool StorageTkEx::checkStorageFormatFile(const std::string pathStr) throw(InvalidConfigException)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   StringMap formatProperties;
   StringMapIter formatIter;

   StorageTk::checkStorageFormatFile(pathStr, STORAGETK_FORMAT_OLD_VERSION,
      STORAGETK_FORMAT_CURRENT_VERSION, &formatProperties);

   formatIter = formatProperties.find(STORAGETK_FORMAT_XATTR);
   if(formatIter == formatProperties.end() )
   {
      throw InvalidConfigException(std::string("Property missing from storage format file: ") +
         STORAGETK_FORMAT_XATTR " (dir: " + pathStr + ")");
      return false;
   }

   bool useXattr = StringTk::strToBool(formatIter->second);
   cfg->setStoreUseExtendedAttribs(useXattr);

   return true;
}

/**
 * Note: intended to be used with fsck only.
 * No locks at all at the moment
 */
FhgfsOpsErr StorageTkEx::getContDirIDsIncremental(unsigned hashDirNum, int64_t lastOffset,
   unsigned maxOutEntries, StringList* outContDirIDs, int64_t* outNewOffset)
{
   const char* logContext = "StorageTkEx (get cont dir ids inc)";
   App* app = Program::getApp();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   unsigned numEntries = 0;
   struct dirent* dirEntry = NULL;

   unsigned firstLevelHashDir;
   unsigned secondLevelHashDir;
   StorageTk::splitHashDirs(hashDirNum, &firstLevelHashDir, &secondLevelHashDir);

   std::string path = StorageTkEx::getMetaDentriesHashDir(
      app->getStructurePath()->getPathAsStrConst(), firstLevelHashDir, secondLevelHashDir);

   DIR* dirHandle = opendir(path.c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr(std::string("Unable to open dentries directory: ") +
         path + ". SysErr: " + System::getErrString() );

      goto return_res;
   }


   errno = 0; // recommended by posix (readdir(3p) )

   // seek to offset
   seekdir(dirHandle, lastOffset); // (seekdir has no return value)

   // the actual entry reading
   for( ; (numEntries < maxOutEntries) && (dirEntry = StorageTk::readdirFiltered(dirHandle) );
       numEntries++)
   {
      std::string dirName = dirEntry->d_name;
      std::string dirID = dirName.substr(0, dirName.length() );

      // skip root dir if this is not the root MDS
      uint16_t rootNodeNumID = Program::getApp()->getMetaNodes()->getRootNodeNumID();
      uint16_t localNodeNumID = Program::getApp()->getLocalNode()->getNumID();
      if (unlikely ( (dirID.compare(META_ROOTDIR_ID_STR) == 0) &&
         (localNodeNumID != rootNodeNumID) ))
         continue;

      //skip disposal
   //   if (unlikely(dirID.compare(META_DISPOSALDIR_ID_STR) == 0))
   //      continue;

      outContDirIDs->push_back(dirID);
      *outNewOffset = dirEntry->d_off;
   }

   if(!dirEntry && errno)
   {
      LogContext(logContext).logErr(std::string("Unable to fetch dentry from: ") +
         path + ". SysErr: " + System::getErrString() );
   }
   else
   { // all entries read
      retVal = FhgfsOpsErr_SUCCESS;
   }


   closedir(dirHandle);

return_res:
   return retVal;
}

/**
 * Note: intended to be used with fsck only
 */
bool StorageTkEx::getNextContDirID(unsigned hashDirNum, int64_t lastOffset, std::string* outID,
   int64_t* outNewOffset)
{
   *outID = "";
   StringList outIDs;
   FhgfsOpsErr retVal = getContDirIDsIncremental(hashDirNum, lastOffset, 1, &outIDs,
      outNewOffset);
   if ( ( outIDs.empty() ) ||  ( retVal != FhgfsOpsErr_SUCCESS ) )
      return false;
   else
   {
      *outID = outIDs.front();
      return true;
   }
}

/**
 * Wrapper/chooser for readInodeFromFileXAttr/Contents.
 *
 * Note: currently unused
 *
 * @param metaFilename the complete path to the inode file
 * @param outFileInode the inode contents (must be deleted by the caller), or NULL if inode is not
 * a file inode
 * @param outDirInode the inode contents (must be deleted by the caller), or NULL if inode is not
 * a dir inode
 * @return the entry type of the inode or DirEntryType_INVALID if an error occured
 */
DirEntryType StorageTkEx::readInodeFromFileUnlocked(std::string& metaFilename,
   FileInode** outFileInode, DirInode** outDirInode)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return readInodeFromFileXAttrUnlocked(metaFilename, outFileInode, outDirInode);
   else
      return readInodeFromFileContentsUnlocked(metaFilename, outFileInode, outDirInode);
}

/**
 * Reads the contents of an inode file (file or dir inode) directly from disk
 *
 * Note: Don't call this directly, use the wrapper loadInodeFromFile().
 * Note: No locking at all
 * Note: currently unused
 *
 * @param metaFilename the complete path to the inode file
 * @param outFileInode the inode contents (must be deleted by the caller), or NULL if inode is not
 * a file inode
 * @param outDirInode the inode contents (must be deleted by the caller), or NULL if inode is not
 * a dir inode
 * @return the entry type of the inode or DirEntryType_INVALID if an error occured
 */
DirEntryType StorageTkEx::readInodeFromFileXAttrUnlocked(std::string& metaFilename,
   FileInode** outFileInode, DirInode** outDirInode)
{
   const char* logContext = "MetaStore (load from xattr file)";

   DirEntryType retVal = DirEntryType_INVALID;

   char *buf = (char*) malloc(META_SERBUF_SIZE);

   ssize_t getRes = getxattr(metaFilename.c_str(), META_XATTR_FILE_NAME, buf, META_SERBUF_SIZE);

   if ( getRes > 0 )
   { // we got something => deserialize it
      retVal = DiskMetaData::deserializeInode(buf, outFileInode, outDirInode);

      if ( unlikely(retVal == DirEntryType_INVALID) )
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize metadata in file: " + metaFilename);
         goto error_freebuf;
      }
   }
   else
   if ( (getRes == -1) && (errno == ENOENT) )
   { // file does not exist
      LOG_DEBUG_CONTEXT(LogContext(logContext), Log_DEBUG,
         "Inode file does not exist: " + metaFilename + ". " + "SysErr: " + System::getErrString());
   }
   else
   { // unhandled error
      LogContext(logContext).logErr(
         "Unable to open/read inode file: " + metaFilename + ". " + "SysErr: "
            + System::getErrString());
   }

   error_freebuf:
   free(buf);

   return retVal;
}

/**
 * Reads the contents of an inode file (file or dir inode) directly from disk
 *
 * Note: Don't call this directly, use the wrapper loadInodeFromFile().
 * Note: No locking at all
 * Note: currently unused
 *
 * @param metaFilename the complete path to the inode file
 * @param outFileInode the inode contents (must be deleted by the caller), or NULL if inode is not
 * a file inode
 * @param outDirInode the inode contents (must be deleted by the caller), or NULL if inode is not
 * a dir inode
 * @return the entry type of the inode or DirEntryType_INVALID if an error occured
 */
DirEntryType StorageTkEx::readInodeFromFileContentsUnlocked(std::string& metaFilename,
   FileInode** outFileInode, DirInode** outDirInode)
{
   const char* logContext = "MetaStore (load from file contents)";

   DirEntryType retVal = DirEntryType_INVALID;

   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(metaFilename.c_str(), openFlags, 0);
   if(fd == -1)
   { // open failed
      if(errno != ENOENT)
         LogContext(logContext).logErr("Unable to open inode file: " + metaFilename + ". " +
            "SysErr: " + System::getErrString() );

      return DirEntryType_INVALID;
   }

   char* buf = (char*)malloc(META_SERBUF_SIZE);
   int readRes = read(fd, buf, META_SERBUF_SIZE);

   if(readRes <= 0)
   { // reading failed
      LogContext(logContext).logErr("Unable to read inode file: " + metaFilename + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   {
      retVal = DiskMetaData::deserializeInode(buf, outFileInode, outDirInode);
      if (unlikely(retVal == DirEntryType_INVALID))
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize inode in file: " + metaFilename);
      }
   }

   free(buf);
   close(fd);

   return retVal;
}
