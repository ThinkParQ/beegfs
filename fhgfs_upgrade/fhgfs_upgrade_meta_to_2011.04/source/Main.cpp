#include "Common.h"
#include "Main.h"
#include "StorageTk.h"
#include "System.h"
#include "MapTk.h"
#include "StorageDefinitions.h"
#include "serialization/Serialization.h"
#include "striping/StripePattern.h"
#include "striping/SimplePattern.h"
#include "striping/Raid0Pattern.h"
#include "MetadataTk.h"

#include <attr/xattr.h>


/* general note: "old" as suffix means 2009.08 storage version, "new" as suffix means 2011.04
   storage version */

#define LOGFILE_PATH                         "/tmp/fhgfs_upgrade_meta." // suffixed with PID
#define LOGFILE_PATH_SUFFIX                  ".log"

#define CONFIG_HASHDIRS_NUM_OLD              (1024)
#define CONFIG_HASHDIRS_NUM_NEW              (8192)
#define CONFIG_ENTRIES_SUBDIR_NAME           "entries"
#define CONFIG_ENTRIES_SUBDIR_NAME_NEW_TMP   "entries.new" /* tmp name to move entries files */
#define CONFIG_STRUCTURE_SUBDIR_NAME         "structure"
#define CONFIG_STRUCTURE_SUBDIR_NAME_NEW_TMP "structure.new" /* tmp name to move structure files */
#define CONFIG_NODEID_FILENAME               "nodeID"
#define CONFIG_TARGETID_FILENAME             "targetID"
#define STORAGE_FORMAT_VERSION_OLD           1
#define FILE_STORAGE_FORMAT_VER_NEW          1
#define DIRECTORY_STORAGE_FORMAT_VER_NEW     1
#define DIRENTRY_STORAGE_FORMAT_VER_NEW      2
#define META_SERBUF_SIZE_OLD                 (4*1024)
#define META_SERBUF_SIZE_NEW                 (8*1024)
#define META_SUBFILE_PREFIX_STR_OLD          "f-"
#define META_SUBDIR_PREFIX_STR_OLD           "d-"
#define META_DIRCONTENTS_EXT_STR_OLD         ".cont"
#define META_UPDATE_EXT_STR_OLD              ".new"
#define META_SUBFILE_SUFFIX_STR_NEW          "-f"
#define META_SUBDIR_SUFFIX_STR_NEW           "-d"
#define META_XATTR_LINK_NAME_NEW             "user.fhgfs_link" /* attribute name for dir-entries */
#define META_XATTR_FILE_NAME_NEW             "user.fhgfs_file" /* attribute name for dir metadata */
#define META_XATTR_DIR_NAME_NEW              "user.fhgfs_dir" /* attribute name for file metadata */


void printUsage(std::string progName)
{
   std::cout << "FhGFS Metadata Server Format Upgrade Tool (2009.08 -> 2011.04)" << std::endl;
   std::cout << "Version: " << FHGFS_VERSION << std::endl;
   std::cout << "http://www.fhgfs.com" << std::endl;
   std::cout << std::endl;
   std::cout << "* Usage..: " << progName << " <metadata_directory> xattr|noxattr" << std::endl;
   std::cout << std::endl;
   std::cout << "* Example: " << progName << " /data/fhgfs_meta xattr" << std::endl;
   std::cout << std::endl;
   std::cout << "* Note...: The option \"xattr\" enables usage of extended attributes to store" << std::endl;
   std::cout << "metadata, the option \"noxattr\" stores metadata as normal file contents. For" << std::endl;
   std::cout << "optimal xattr performance on ext4, inodes must at least have 512 bytes size." << std::endl;
   std::cout << "Ext3/4 might also require mounting with \"-ouser_xattr\" to support extended" << std::endl;
   std::cout << "attributes." << std::endl;
   std::cout << std::endl;
   std::cout << "BACKUP YOUR METADATA BEFORE STARTING THE UPGRADE PROCESS!" << std::endl;
   std::cout << std::endl;
}

/**
 * Logs to log file and prints to stdout or stderr (depending on value of isError).
 */
void logMsg(std::string msg, bool isError, bool appendNewLine)
{
   std::string logPath = std::string(LOGFILE_PATH) + StringTk::uint64ToStr(System::getPID() ) +
      LOGFILE_PATH_SUFFIX;

   if(appendNewLine)
   {
      if(isError)
         std::cerr << msg << std::endl;
      else
         std::cout << msg << std::endl;
   }
   else
   {
      if(isError)
         std::cerr << msg << std::flush;
      else
         std::cout << msg << std::flush;
   }

   std::ofstream file(logPath.c_str(), std::ios_base::out | std::ios_base::app);

   if(!file.is_open() || file.fail() )
      return;

   if(appendNewLine)
      file << msg << std::endl;
   else
      file << msg;

   file.close();
}

void logErrMsg(std::string msg)
{
   logMsg(msg, true, true);
}

void logStdMsg(std::string msg, bool appendNewLine=true)
{
   logMsg(msg, false, appendNewLine);
}

bool checkAndLockStorageDir(std::string dir)
{
   logStdMsg("* Checking storage directory...");

   if(!StorageTk::pathExists(dir) )
   {
      logErrMsg("Directory not found: " + dir);
      return false;
   }

   // storage format file checks

   if(!StorageTk::checkStorageFormatFileExists(dir) )
   {
      logErrMsg("Storage format file not found in directory: " + dir);
      return false;
   }

   if(!StorageTk::checkStorageFormatFile(dir, STORAGE_FORMAT_VERSION_OLD) )
   {
      logErrMsg("Invalid storage format file in directory: " + dir);
      return false;
   }

   // entries dir checks

   std::string entriesDir = dir + "/" + CONFIG_ENTRIES_SUBDIR_NAME;
   if(!StorageTk::pathExists(entriesDir) )
   {
      logErrMsg("Entries directory not found: " + entriesDir);
      return false;
   }

   std::string firstEntriesHashDir =
      entriesDir + "/" + StringTk::uintToHexStr(0);
   if(!StorageTk::pathExists(firstEntriesHashDir) )
   {
      logErrMsg("Entries hash directory not found: " + firstEntriesHashDir);
      return false;
   }

   std::string tooHighEntriesHashDir =
      entriesDir + "/" + StringTk::uintToHexStr(CONFIG_HASHDIRS_NUM_OLD);
   if(StorageTk::pathExists(tooHighEntriesHashDir) )
   {
      /* note: 2009.08 fomat has only hash dirs 0..1023 (in hex), so we check for hash dir 1024,
         which shouldn't exist here (but was introduced in the 2011.04 storage format) */
      logErrMsg("Found invalid entries hash directory: " + tooHighEntriesHashDir);
      return false;
   }

   // structure dir checks

   std::string structureDir = dir + "/" + CONFIG_STRUCTURE_SUBDIR_NAME;
   if(!StorageTk::pathExists(structureDir) )
   {
      logErrMsg("Structure directory not found: " + structureDir);
      return false;
   }

   std::string firstStructureHashDir =
      structureDir + "/" + StringTk::uintToHexStr(0);
   if(!StorageTk::pathExists(firstStructureHashDir) )
   {
      logErrMsg("Entries hash directory not found: " + firstStructureHashDir);
      return false;
   }

   std::string tooHighStructureHashDir =
      entriesDir + "/" + StringTk::uintToHexStr(CONFIG_HASHDIRS_NUM_OLD);
   if(StorageTk::pathExists(tooHighStructureHashDir) )
   {
      /* note: 2009.08 fomat has only hash dirs 0..1023 (in hex), so we check for hash dir 1024,
         which shouldn't exist here (but was introduced in the 2011.04 storage format) */
      logErrMsg("Found invalid structure hash directory: " + tooHighStructureHashDir);

      return false;
   }

   // lock storage dir

   /* (note: we will never unlock this dir, because system will do it for us on exit and we don't
      really want to care about the cleanup code in this tool.) */
   int lockFD = StorageTk::lockWorkingDirectory(dir);
   if(lockFD == -1)
   {
      logErrMsg("Unable to lock storage directory:" + dir);
      return false;
   }

   return true;
}

bool createEntriesTmpDir(std::string storageDir)
{
   logStdMsg("* Creating new entries directory... (" CONFIG_ENTRIES_SUBDIR_NAME_NEW_TMP ")");

   // chunks directory
   Path entriesPathTmp(storageDir + "/" CONFIG_ENTRIES_SUBDIR_NAME_NEW_TMP);

   if(!StorageTk::createPathOnDisk(entriesPathTmp, false) )
   {
      logErrMsg("Unable to create new entries directory: " + entriesPathTmp.getPathAsStr() );
      return false;
   }

   // chunks subdirs
   for(unsigned i=0; i < CONFIG_HASHDIRS_NUM_NEW; i++)
   {
      std::string subdirName = StringTk::uintToHexStr(i);
      Path subdirPath(entriesPathTmp, subdirName);

      if(!StorageTk::createPathOnDisk(subdirPath, false) )
      {
         logErrMsg("Unable to create new entries hash directory: " + subdirPath.getPathAsStr() );
         return false;
      }
   }

   return true;
}

bool createStructureTmpDir(std::string storageDir)
{
   logStdMsg("* Creating new structure directory... (" CONFIG_STRUCTURE_SUBDIR_NAME_NEW_TMP ")");

   // chunks directory
   Path structurePathTmp(storageDir + "/" CONFIG_STRUCTURE_SUBDIR_NAME_NEW_TMP);

   if(!StorageTk::createPathOnDisk(structurePathTmp, false) )
   {
      logErrMsg("Unable to create new chunks directory: " + structurePathTmp.getPathAsStr() );
      return false;
   }

   // chunks subdirs
   for(unsigned i=0; i < CONFIG_HASHDIRS_NUM_NEW; i++)
   {
      std::string subdirName = StringTk::uintToHexStr(i);
      Path subdirPath(structurePathTmp, subdirName);

      if(!StorageTk::createPathOnDisk(subdirPath, false) )
      {
         logErrMsg("Unable to create new structure hash directory: " + subdirPath.getPathAsStr() );
         return false;
      }
   }

   return true;
}

/**
 * @param outEntryID only valid if true is returned
 * @return false if problems detected during deserialization
 */
bool migrateFileEntryBuf(char* oldBuf, unsigned oldBufLen, char* outNewBuf, unsigned* outNewBufLen,
   std::string* outEntryID)
{
   std::string id; // filesystem-wide unique string
   StripePattern* stripePattern = NULL;
   unsigned userID;
   unsigned groupID;
   int mode;
   int64_t creationTimeSecs; // secs since the epoch
   unsigned numHardlinks; // not used yet

   // STAGE 1: deserialize old metadata buf

   size_t bufPos = 0;

   // metadata format version
   unsigned formatVersionFieldLen;
   unsigned formatVersionBuf; // we have only one format

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &formatVersionBuf,
      &formatVersionFieldLen) )
      goto err_cleanup;

   bufPos += formatVersionFieldLen;

   // ID
   unsigned idBufLen;
   unsigned idLen;
   const char* idStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &idLen, &idStrStart, &idBufLen) )
      goto err_cleanup;

   id = idStrStart;
   bufPos += idBufLen;

   // stripePattern
   unsigned patternBufLen;
   StripePatternHeader patternHeader;
   const char* patternStart;

   if(!StripePattern::deserializePatternPreprocess(&oldBuf[bufPos], oldBufLen-bufPos,
      &patternHeader, &patternStart, &patternBufLen) )
      goto err_cleanup;

   stripePattern = StripePattern::createFromBuf(patternStart, patternHeader);
   bufPos += patternBufLen;

   // userID
   unsigned userFieldLen;

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &userID, &userFieldLen) )
      goto err_cleanup;

   bufPos += userFieldLen;

   // groupID
   unsigned groupFieldLen;

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &groupID, &groupFieldLen) )
      goto err_cleanup;

   bufPos += groupFieldLen;

   // mode
   unsigned modeFieldLen;

   if(!Serialization::deserializeInt(&oldBuf[bufPos], oldBufLen-bufPos, &mode, &modeFieldLen) )
      goto err_cleanup;

   bufPos += modeFieldLen;

   // creationTimeSecs
   unsigned creationFieldLen;

   if(!Serialization::deserializeInt64(&oldBuf[bufPos], oldBufLen-bufPos, &creationTimeSecs,
      &creationFieldLen) )
      goto err_cleanup;

   bufPos += creationFieldLen;

   // numHardlinks
   unsigned numHardlinksLen;

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &numHardlinks,
      &numHardlinksLen) )
      goto err_cleanup;

   bufPos += numHardlinksLen;


   // STAGE 2: serialize new metadata buf

   bufPos = 0;

   // metadata format version
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], FILE_STORAGE_FORMAT_VER_NEW);

   // feature flags (doesn't exist in old format)
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 0);

   // creationTimeSecs
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], creationTimeSecs);

   // attribChangeTimeSecs (doesn't exist in old format)
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], 0);

   // modificationTimeSecs (doesn't exist in old format)
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], 0);

   // lastAccessTimeSecs (doesn't exist in old format)
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], 0);

   // fileLength (doesn't exist in old format)
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], 0);

   // numHardlinks
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 1);

   // contentsVersion (doesn't exist in old format)
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 0);

   // userID
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], userID);

   // groupID
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], groupID);

   // mode
   bufPos += Serialization::serializeInt(&outNewBuf[bufPos], mode);

   // ID
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], id.size(), id.c_str() );

   // parentDirID (just an unreliable hint; doesn't exist in old format)
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], 0, "");

   // parentNodeID (just an unreliable hint; doesn't exist in old format)
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], 0, "");

   // stripePattern
   bufPos += stripePattern->serialize(&outNewBuf[bufPos]);


   *outNewBufLen = bufPos;
   *outEntryID = id;
   delete(stripePattern);

   return true;


err_cleanup:
   SAFE_DELETE(stripePattern);

   return false;
}

/**
 * @param outEntryID only valid if true is returned
 * @return false if problems detected during deserialization
 */
bool migrateDirEntryBuf(char* oldBuf, unsigned oldBufLen, char* outNewBuf, unsigned* outNewBufLen,
   std::string* outEntryID)
{
   std::string id; // a filesystem-wide identifier for this dir
   std::string ownerNodeID; // empty for unknown owner
   StripePattern* stripePattern = NULL; // is the default for new files and subdirs

   unsigned userID;
   unsigned groupID;
   int mode;
   int64_t creationTimeSecs; // secs since the epoch
   int64_t modificationTimeSecs; // secs since the epoch
   int64_t lastAccessTimeSecs; // secs since the epoch

   // STAGE 1: deserialize old metadata buf

   size_t bufPos = 0;

   // metadata format version
   unsigned formatVersionFieldLen;
   unsigned formatVersionBuf; // we have only one format

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &formatVersionBuf,
      &formatVersionFieldLen) )
      goto err_cleanup;

   bufPos += formatVersionFieldLen;

   // ID
   unsigned idBufLen;
   unsigned idLen;
   const char* idStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &idLen, &idStrStart, &idBufLen) )
      goto err_cleanup;

   id = idStrStart;
   bufPos += idBufLen;

   // ownerNodeID
   unsigned ownerBufLen;
   unsigned ownerLen;
   const char* ownerStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &ownerLen, &ownerStrStart, &ownerBufLen) )
      goto err_cleanup;

   ownerNodeID = ownerStrStart;
   bufPos += ownerBufLen;

   // stripePattern
   unsigned patternBufLen;
   StripePatternHeader patternHeader;
   const char* patternStart;

   if(!StripePattern::deserializePatternPreprocess(&oldBuf[bufPos], oldBufLen-bufPos,
      &patternHeader, &patternStart, &patternBufLen) )
      goto err_cleanup;

   stripePattern = StripePattern::createFromBuf(patternStart, patternHeader);
   bufPos += patternBufLen;

   // userID
   unsigned userFieldLen;

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &userID, &userFieldLen) )
      goto err_cleanup;

   bufPos += userFieldLen;

   // groupID
   unsigned groupFieldLen;

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &groupID, &groupFieldLen) )
      goto err_cleanup;

   bufPos += groupFieldLen;

   // mode
   unsigned modeFieldLen;

   if(!Serialization::deserializeInt(&oldBuf[bufPos], oldBufLen-bufPos, &mode, &modeFieldLen) )
      goto err_cleanup;

   bufPos += modeFieldLen;

   // creationTimeSecs
   unsigned creationFieldLen;

   if(!Serialization::deserializeInt64(&oldBuf[bufPos], oldBufLen-bufPos, &creationTimeSecs,
      &creationFieldLen) )
      goto err_cleanup;

   bufPos += creationFieldLen;

   // modificationTimeSecs
   unsigned modificationFieldLen;

   if(!Serialization::deserializeInt64(&oldBuf[bufPos], oldBufLen-bufPos, &modificationTimeSecs,
      &modificationFieldLen) )
      goto err_cleanup;

   bufPos += modificationFieldLen;

   // lastAccessTimeSecs
   unsigned accessFieldLen;

   if(!Serialization::deserializeInt64(&oldBuf[bufPos], oldBufLen-bufPos, &lastAccessTimeSecs,
      &accessFieldLen) )
      goto err_cleanup;

   bufPos += accessFieldLen;


   // STAGE 2: serialize new metadata buf

   bufPos = 0;

   // metadata format version
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], DIRECTORY_STORAGE_FORMAT_VER_NEW);

   // feature flags (currently unused)
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 0);

   // creationTimeSecs
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], creationTimeSecs);

   // attribChangeTimeSecs (did exist only as mtime in old format)
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], modificationTimeSecs);

   // modificationTimeSecs
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], modificationTimeSecs);

   // lastAccessTimeSecs
   bufPos += Serialization::serializeInt64(&outNewBuf[bufPos], lastAccessTimeSecs);

   // userID
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], userID);

   // groupID
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], groupID);

   // mode
   bufPos += Serialization::serializeInt(&outNewBuf[bufPos], mode);

   // numSubdirs (doesn't exist in old format)
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 0);

   // numFiles (doesn't exist in old format)
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 0);

   // ID
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], id.size(), id.c_str() );

   // ownerNodeID
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], ownerNodeID.size(),
      ownerNodeID.c_str() );

   // parentDirID (just an unreliable hint; doesn't exist in old format)
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], 0, "");

   // parentNodeID (just an unreliable hint; doesn't exist in old format)
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], 0, "");

   // stripePattern
   bufPos += stripePattern->serialize(&outNewBuf[bufPos]);


   *outNewBufLen = bufPos;
   *outEntryID = id;
   delete(stripePattern);

   return true;


err_cleanup:
   SAFE_DELETE(stripePattern);

   return false;
}

DirEntryType getFileTypeFromOldMetadataBuf(const char* buf, unsigned bufLen)
{
   // note: we start here like we do with normal deserialization and just stop at the "mode" field.

   std::string id; // filesystem-wide unique string
   unsigned userID;
   unsigned groupID;
   int mode;

   // STAGE 1: deserialize old metadata buf

   size_t bufPos = 0;

   // metadata format version
   unsigned formatVersionFieldLen;
   unsigned formatVersionBuf; // we have only one format

   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &formatVersionBuf,
      &formatVersionFieldLen) )
      goto err_cleanup;

   bufPos += formatVersionFieldLen;

   // ID
   unsigned idBufLen;
   unsigned idLen;
   const char* idStrStart;

   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &idLen, &idStrStart, &idBufLen) )
      goto err_cleanup;

   id = idStrStart;
   bufPos += idBufLen;

   // stripePattern
   unsigned patternBufLen;
   StripePatternHeader patternHeader;
   const char* patternStart;

   if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
      &patternHeader, &patternStart, &patternBufLen) )
      goto err_cleanup;

   bufPos += patternBufLen;

   // userID
   unsigned userFieldLen;

   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &userID, &userFieldLen) )
      goto err_cleanup;

   bufPos += userFieldLen;

   // groupID
   unsigned groupFieldLen;

   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &groupID, &groupFieldLen) )
      goto err_cleanup;

   bufPos += groupFieldLen;

   // mode
   unsigned modeFieldLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &mode, &modeFieldLen) )
      goto err_cleanup;

   bufPos += modeFieldLen;


   return MetadataTk::posixFileTypeToDirEntryType(mode);


err_cleanup:

   return DirEntryType_INVALID;
}

/**
 * allocs a new buf and reads the contents of the given file into the alloc'ed buffer.
 *
 * @return alloc'ed buffer, which must be free'd by the caller; or NULL on error.
 */
char* readFileBufAlloc(std::string path, unsigned bufLen, unsigned* outReadLen)
{
   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(path.c_str(), openFlags, 0);
   if(fd == -1)
   { // open failed
      logErrMsg("Unable to open metadata file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      return NULL;
   }

   char* buf = (char*)malloc(bufLen);
   int readRes = read(fd, buf, bufLen);
   if(readRes < 0)
   { // reading failed
      logErrMsg("Unable to read metadata file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      free(buf);
      close(fd);
      return NULL;
   }

   close(fd);

   *outReadLen = readRes;
   return buf;
}

bool writeFileBuf(std::string path, const char* buf, unsigned bufLen, bool useXAttr,
   const char* xattrName)
{
   // create metadata file
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(path.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      logErrMsg("Unable to create metadata file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      goto error_donothing;
   }

   // write data to file

   if(useXAttr)
   { // extended attribute
      int setRes = fsetxattr(fd, xattrName, buf, bufLen, 0);

      if(unlikely(setRes == -1) )
      { // error
         logErrMsg("Unable to store file xattr metadata: " + path + ". " +
            "SysErr: " + System::getErrString() );

         goto error_closefile;
      }
   }
   else
   { // normal file content
      ssize_t writeRes = write(fd, buf, bufLen);

      if(writeRes != (ssize_t)bufLen)
      {
         logErrMsg("Unable to write file metadata: " + path + ". " +
            "SysErr: " + System::getErrString() );

         goto error_closefile;
      }
   }

   close(fd);

   return true;


   // error compensation
error_closefile:
   close(fd);
   unlink(path.c_str() );

error_donothing:

   return false;
}

/**
 * We need this reader to determine the exact entry type (e.g. regular file or symlink) for the new
 * entry links.
 */
DirEntryType readFileTypeFromOldMetadata(std::string storageDir, std::string id)
{
   std::string entryFilename = META_SUBFILE_PREFIX_STR_OLD + id;
   unsigned entryHashDir = StringTk::strChecksumOld(entryFilename.c_str(),
      entryFilename.size() ) % CONFIG_HASHDIRS_NUM_OLD;

   std::string storageTmpEntriesDir = storageDir + "/" + CONFIG_ENTRIES_SUBDIR_NAME;
   std::string entryPath = storageTmpEntriesDir + "/" + StringTk::uintToHexStr(entryHashDir) + "/" +
      entryFilename;

   unsigned bufLen = META_SERBUF_SIZE_OLD;
   unsigned readLen;

   char* buf = readFileBufAlloc(entryPath, bufLen, &readLen);

   if(!buf || !readLen)
      return DirEntryType_INVALID;

   DirEntryType entryType = getFileTypeFromOldMetadataBuf(buf, readLen);

   free(buf);

   return entryType;
}

/**
 * @param ownerNodeID local ID from nodeID file or hostname (did not exist in old format, but is
 * required in new format)
 * @param outLinkName only valid if true is returned
 * @return false if problems detected during deserialization
 */
bool migrateFileLinkBuf(std::string storageDir, std::string ownerNodeID, char* oldBuf,
   unsigned oldBufLen, char* outNewBuf, unsigned* outNewBufLen, std::string* outLinkName)
{
   std::string name; // user-friendly name
   std::string id; // filesystem-wide unique string

   // STAGE 1: deserialize old metadata buf

   size_t bufPos = 0;

   // metadata format version
   unsigned formatVersionFieldLen;
   unsigned formatVersionBuf; // we have only one format

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &formatVersionBuf,
      &formatVersionFieldLen) )
      return false;

   bufPos += formatVersionFieldLen;

   // name
   unsigned nameBufLen;
   unsigned nameLen;
   const char* nameStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &nameLen, &nameStrStart, &nameBufLen) )
      return false;

   name = nameStrStart;
   bufPos += nameBufLen;

   // ID
   unsigned idBufLen;
   unsigned idLen;
   const char* idStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &idLen, &idStrStart, &idBufLen) )
      return false;

   id = idStrStart;
   bufPos += idBufLen;


   // STAGE pre-2: find out precise entry type for new link type info

   DirEntryType entryType = readFileTypeFromOldMetadata(storageDir, id);
   if(entryType == DirEntryType_INVALID)
      return false;


   // STAGE 2: serialize new metadata buf

   bufPos = 0;

   // metadata format version
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], DIRENTRY_STORAGE_FORMAT_VER_NEW);

   // feature flags (currently unused)
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 0);

   // entryType
   // (note: we have a fixed position for the entryType byte: DIRENTRY_TYPE_BUF_POS)
   bufPos += Serialization::serializeChar(&outNewBuf[bufPos], entryType);

   // 7 bytes padding for 8 byte alignment
   bufPos += 7;

   // name
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], name.size(), name.c_str() );

   // ID
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], id.size(), id.c_str() );

   // ownerNodeID
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], ownerNodeID.size(),
      ownerNodeID.c_str() );


   *outNewBufLen = bufPos;
   *outLinkName = name;

   return true;
}


/**
 * @param outLinkName only valid if true is returned
 * @return false if problems detected during deserialization
 */
bool migrateDirLinkBuf(char* oldBuf, unsigned oldBufLen, char* outNewBuf, unsigned* outNewBufLen,
   std::string* outLinkName)
{
   std::string name; // the user-friendly name
   std::string id; // a filesystem-wide identifier for this dir
   std::string ownerNodeID;

   // STAGE 1: deserialize old metadata buf

   size_t bufPos = 0;

   // metadata format version
   unsigned formatVersionFieldLen;
   unsigned formatVersionBuf; // we have only one format currently

   if(!Serialization::deserializeUInt(&oldBuf[bufPos], oldBufLen-bufPos, &formatVersionBuf,
      &formatVersionFieldLen) )
      return false;

   bufPos += formatVersionFieldLen;

   // name
   unsigned nameBufLen;
   unsigned nameLen;
   const char* nameStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &nameLen, &nameStrStart, &nameBufLen) )
      return false;

   name = nameStrStart;
   bufPos += nameBufLen;

   // ID
   unsigned idBufLen;
   unsigned idLen;
   const char* idStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &idLen, &idStrStart, &idBufLen) )
      return false;

   id = idStrStart;
   bufPos += idBufLen;

   // ownerNodeID
   unsigned ownerBufLen;
   unsigned ownerLen;
   const char* ownerStrStart;

   if(!Serialization::deserializeStr(&oldBuf[bufPos], oldBufLen-bufPos,
      &ownerLen, &ownerStrStart, &ownerBufLen) )
      return false;

   ownerNodeID = ownerStrStart;
   bufPos += ownerBufLen;


   // STAGE 2: serialize new metadata buf

   bufPos = 0;

   // metadata format version
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], DIRENTRY_STORAGE_FORMAT_VER_NEW);

   // feature flags (currently unused)
   bufPos += Serialization::serializeUInt(&outNewBuf[bufPos], 0);

   // entryType
   // (note: we have a fixed position for the entryType byte: DIRENTRY_TYPE_BUF_POS)
   bufPos += Serialization::serializeChar(&outNewBuf[bufPos], DirEntryType_DIRECTORY);

   // 7 bytes padding for 8 byte alignment
   bufPos += 7;

   // name
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], name.size(), name.c_str() );

   // ID
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], id.size(), id.c_str() );

   // ownerNodeID
   bufPos += Serialization::serializeStrAlign4(&outNewBuf[bufPos], ownerNodeID.size(),
      ownerNodeID.c_str() );


   *outNewBufLen = bufPos;
   *outLinkName = name;

   return true;
}

/**
 * Converts the metadata files in the entries-subdir to the new format and stores them in a new
 * temporary directory.
 */
bool migrateEntryFilesToTmpDir(std::string storageDir, bool useXAttr)
{
   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   std::string pathOld = storageDir + "/" CONFIG_ENTRIES_SUBDIR_NAME;
   std::string pathNew = storageDir + "/" CONFIG_ENTRIES_SUBDIR_NAME_NEW_TMP;

   char newBuf[META_SERBUF_SIZE_NEW]; // buf for new entry serialization

   logStdMsg("* Migrating metadata entry files... (Processing " +
      StringTk::uint64ToStr(CONFIG_HASHDIRS_NUM_OLD) + " hash directories...)");
   logStdMsg("* Progress: ", false);

   // walk over all old entries subdirs
   for(unsigned i=0; i < CONFIG_HASHDIRS_NUM_OLD; i++)
   {
      std::string subdirNameOld = StringTk::uintToHexStr(i);
      std::string subdirPathOld = pathOld + "/" + subdirNameOld;

      logStdMsg(subdirNameOld + " ", false); // print dir num as progress info (without newline)

      DIR* dirHandle = opendir(subdirPathOld.c_str() );
      if(!dirHandle)
      {
         logErrMsg("Unable to open hash directory: " + subdirPathOld + ". " +
            "SysErr: " + System::getErrString() );

         goto err_cleanup;
      }


      errno = 0; // recommended by posix (readdir(3p) )

      // read all entries and move them to new tmp dir
      while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
      {
         std::string filename = dirEntry->d_name;
         std::string filepathOld = subdirPathOld + "/" + filename;

         char* oldBuf = NULL;
         bool migrateBufRes;
         std::string entryID;
         std::string filenameNew;
         unsigned readLen;
         const char* xattrName;
         unsigned newBufSerLen = 0; // used buffer len for serialization

         if(strstr(filename.c_str(), META_UPDATE_EXT_STR_OLD) )
            goto unlink_old_file; // temporary update file => ignore


         if(!strncmp(filename.c_str(), META_SUBFILE_PREFIX_STR_OLD,
            strlen(META_SUBFILE_PREFIX_STR_OLD) ) )
         { // file metadata
            oldBuf = readFileBufAlloc(filepathOld, META_SERBUF_SIZE_OLD, &readLen);
            if(!oldBuf)
            { // just openening and reading a file should never fail, so this error is critical
               goto err_closedir;
            }

            migrateBufRes = migrateFileEntryBuf(oldBuf, readLen, newBuf, &newBufSerLen, &entryID);
            filenameNew = entryID + META_SUBFILE_SUFFIX_STR_NEW;
            xattrName = META_XATTR_FILE_NAME_NEW;
         }
         else
         if(!strncmp(filename.c_str(), META_SUBDIR_PREFIX_STR_OLD,
            strlen(META_SUBDIR_PREFIX_STR_OLD) ) )
         { // dir metadata
            oldBuf = readFileBufAlloc(filepathOld, META_SERBUF_SIZE_OLD, &readLen);
            if(!oldBuf)
            { // just openening and reading a file should never fail, so this error is critical
               goto err_closedir;
            }

            migrateBufRes = migrateDirEntryBuf(oldBuf, readLen, newBuf, &newBufSerLen, &entryID);
            filenameNew = entryID + META_SUBDIR_SUFFIX_STR_NEW;
            xattrName = META_XATTR_DIR_NAME_NEW;
         }
         else
            goto unlink_old_file; // unknown metadata file type => ignore


         if(!migrateBufRes)
         { // erroneous input file (might just be a residue from server crash, so we ignore it)
            free(oldBuf);
            goto unlink_old_file;
         }

         { // just a separate frame to enable goto jumps accross variable initializations
            unsigned nameChecksumNew = StringTk::strChecksumNew(filenameNew.c_str(),
               filenameNew.length() ) % CONFIG_HASHDIRS_NUM_NEW;

            std::string subdirNameNew = StringTk::uintToHexStr(nameChecksumNew);
            std::string filePathNew = pathNew + "/" + subdirNameNew + "/" + filenameNew;


            bool writeFileRes = writeFileBuf(
               filePathNew, newBuf, newBufSerLen, useXAttr, xattrName);

            free(oldBuf);

            if(!writeFileRes)
            { // definitely critical error => remove new file and terminate
               unlink(filePathNew.c_str() );
               goto err_closedir;
            }
         }

         numEntries++; // (we only count successful entries here)

         // new file written => unlink old file
      unlink_old_file:
         int unlinkOldFileRes = unlink(filepathOld.c_str() );
         if(unlinkOldFileRes)
         { // unable to unlink old file. not critical, so we don't treat it as reason to cancel.
            logErrMsg("Unable to unlink old file: " + filepathOld + ". " +
               "SysErr: " + System::getErrString() );
         }

      } // end of "while(dirEntry = ...)"

      if(!dirEntry && errno)
      {
         logErrMsg("Unable to fetch directory entry from: " + pathOld + ". " +
            "SysErr: " + System::getErrString() );

         goto err_closedir;
      }

      closedir(dirHandle);
      continue;

   err_closedir:
      closedir(dirHandle);
      goto err_cleanup;

   } // end of "for each hashdir" loop


   logStdMsg(""); // finalizing endl for dir progress

   logStdMsg("* Converted metadata files: " + StringTk::uint64ToStr(numEntries) );


   // all entries read/moved
   return true;


err_cleanup:

   return false;
}


/**
 * Migrates entry links living in a subdir of a structure hash dir.
 *
 * @path path to old structure subdir (where we will read the link files).
 * @newTmpPath path to new temporary structure subdir (where we will put the new link files);
 * will be created.
 */
bool migrateStructureSubdirToTmpDir(std::string storageDir, std::string nodeID, std::string oldPath,
   std::string newTmpPath, bool useXAttr)
{
   // create the new .cont directory in the new temporary structure dir


   int mkdirRes = mkdir(newTmpPath.c_str(), 0777);
   if(mkdirRes && (errno != EEXIST) )
   {
      logErrMsg("Unable to create new structure subdir: " + newTmpPath + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   // walk the old .cont dir and migrate each contained file

   DIR* dirHandle = opendir(oldPath.c_str() );
   if(!dirHandle)
   {
      logErrMsg("Unable to open structure subdir: " + oldPath + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   char newBuf[META_SERBUF_SIZE_NEW]; // buf for new entry serialization

   errno = 0; // recommended by posix (readdir(3p) )

   // read all entries and move them to new tmp dir
   while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
   {
      std::string filename = dirEntry->d_name;
      std::string filepathOld = oldPath + "/" + filename;

      char* oldBuf = NULL;
      bool migrateBufRes;
      std::string entryID;
      unsigned readLen;
      const char* xattrName;
      unsigned newBufSerLen = 0; // used buffer len for serialization

      if(strstr(filename.c_str(), META_UPDATE_EXT_STR_OLD) )
         goto unlink_old_file; // temporary update file => ignore


      if(!strncmp(filename.c_str(), META_SUBFILE_PREFIX_STR_OLD,
         strlen(META_SUBFILE_PREFIX_STR_OLD) ) )
      { // file link
         oldBuf = readFileBufAlloc(filepathOld, META_SERBUF_SIZE_OLD, &readLen);
         if(!oldBuf)
         { // just openening and reading a file should never fail, so this error is critical
            goto err_closedir;
         }

         migrateBufRes = migrateFileLinkBuf(
            storageDir, nodeID, oldBuf, readLen, newBuf, &newBufSerLen, &entryID);
      }
      else
      if(!strncmp(filename.c_str(), META_SUBDIR_PREFIX_STR_OLD,
         strlen(META_SUBDIR_PREFIX_STR_OLD) ) )
      { // dir link
         oldBuf = readFileBufAlloc(filepathOld, META_SERBUF_SIZE_OLD, &readLen);
         if(!oldBuf)
         { // just openening and reading a file should never fail, so this error is critical
            goto err_closedir;
         }

         migrateBufRes = migrateDirLinkBuf(oldBuf, readLen, newBuf, &newBufSerLen, &entryID);
      }
      else
         goto unlink_old_file; // unknown metadata file type => ignore


      if(!migrateBufRes)
      { // erroneous input file (might just be a residue from server crash, so we ignore it)
         free(oldBuf);
         goto unlink_old_file;
      }

      { // just a separate frame to enable goto jumps accross variable initializations
         std::string filenameNew = entryID;
         xattrName = META_XATTR_LINK_NAME_NEW;

         std::string filePathNew = newTmpPath + "/" + filenameNew;


         bool writeFileRes = writeFileBuf(filePathNew, newBuf, newBufSerLen, useXAttr, xattrName);

         free(oldBuf);

         if(!writeFileRes)
         { // definitely critical error => remove new file and terminate
            unlink(filePathNew.c_str() );
            goto err_closedir;
         }
      }


      numEntries++; // (we only count successful entries here)

      // new file written => unlink old file
   unlink_old_file:
      int unlinkOldFileRes = unlink(filepathOld.c_str() );
      if(unlinkOldFileRes)
      { // unable to unlink old file. not critical, so we don't treat it as reason to cancel.
         logErrMsg("Unable to unlink old file: " + filepathOld + ". " +
            "SysErr: " + System::getErrString() );
      }

   } // end of "while(dirEntry = ...)" loop

   if(!dirEntry && errno)
   {
      logErrMsg("Unable to fetch directory entry from: " + oldPath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_closedir;
   }

   closedir(dirHandle);

   { // just a separate frame to enable goto jumps accross variable initializations
      // remove old directory
      int rmOldDirRes = rmdir(oldPath.c_str() );
      if(rmOldDirRes)
      { // this error is not critical
         logErrMsg("Unable to remove old directory: " + oldPath + ". " +
            "SysErr: " + System::getErrString() );
      }
   }


   return true;


err_closedir:
   closedir(dirHandle);

   return false;
}

/**
 * Converts the directory entry link files in the structure-subdir to the new format and stores them
 * in a new temporary directory.
 *
 * Note: Structure dir walk must be done before entries dir migration, because it access some
 * files in the old entries directory to fill the new extended link information (=> entry types).
 */
bool walkStructureDirs(std::string storageDir, std::string nodeID, bool useXAttr)
{
   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   std::string pathOld = storageDir + "/" CONFIG_STRUCTURE_SUBDIR_NAME;
   std::string pathNew = storageDir + "/" CONFIG_STRUCTURE_SUBDIR_NAME_NEW_TMP;

   logStdMsg("* Migrating directory entry link files... (Processing " +
      StringTk::uint64ToStr(CONFIG_HASHDIRS_NUM_OLD) + " hash directories...)" );
   logStdMsg("* Progress: ", false);

   // walk over all old structure hash subdirs
   for(unsigned i=0; i < CONFIG_HASHDIRS_NUM_OLD; i++)
   {
      std::string subdirNameOld = StringTk::uintToHexStr(i);
      std::string subdirPathOld = pathOld + "/" + subdirNameOld;

      logStdMsg(subdirNameOld + " ", false); // print dir num as progress info (without newline)

      DIR* dirHandle = opendir(subdirPathOld.c_str() );
      if(!dirHandle)
      {
         logErrMsg("Unable to open hash directory: " + subdirPathOld + ". " +
            "SysErr: " + System::getErrString() );

         goto err_cleanup;
      }


      // read all entries and move them to new tmp dir
      while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
      {
         std::string contentsDirName = dirEntry->d_name;
         std::string contentsDirPath = subdirPathOld + "/" + contentsDirName;

         if(!strstr(contentsDirName.c_str(), META_DIRCONTENTS_EXT_STR_OLD) )
            continue; // not a contents directory => ignore

         std::string contentsDirNameNoPrefix =
            contentsDirName.substr(sizeof(META_SUBDIR_PREFIX_STR_OLD) -1); // -1 for terminating '0'
         unsigned contentsDirChecksumNew = StringTk::strChecksumNew(contentsDirNameNoPrefix.c_str(),
            contentsDirNameNoPrefix.length() ) % CONFIG_HASHDIRS_NUM_NEW;
         std::string contentsDirChecksumStrNew = StringTk::uintToHexStr(contentsDirChecksumNew);

         std::string contentsDirPathNew = pathNew + "/" + contentsDirChecksumStrNew + "/" +
            contentsDirNameNoPrefix;

         bool migrateRes = migrateStructureSubdirToTmpDir(
            storageDir, nodeID, contentsDirPath, contentsDirPathNew, useXAttr);

         if(!migrateRes)
         {
            logErrMsg("Unable to convert directory: " + contentsDirPath + ".");
            goto err_closedir;
         }

         numEntries++;
      }

      if(!dirEntry && errno)
      {
         logErrMsg("Unable to fetch directory entry from: " + pathOld + ". " +
            "SysErr: " + System::getErrString() );

         goto err_closedir;
      }

      closedir(dirHandle);

      continue;

   err_closedir:
      closedir(dirHandle);
      goto err_cleanup;
   }


   logStdMsg(""); // finalizing endl for dir progress

   logStdMsg("* Converted user directories: " + StringTk::uint64ToStr(numEntries) );

   // all entries read/moved
   return true;


err_cleanup:

   return false;
}

/**
 * @param subdirName is the current name of the directory which will be renamed to "<current>.old"
 * and then completely removed afterwards.
 * @param newSubdirTmpName is the name of the new replacement directory that shall be renamed to
 * subdirName.
 */
bool switchDirToNew(std::string storageDir, std::string subdirName, std::string newSubdirTmpName)
{
   std::string pathOrig = storageDir + "/" + subdirName;
   std::string pathNewTmp = storageDir + "/" + newSubdirTmpName;
   std::string pathOldTmp = storageDir + "/" + subdirName + ".old";

   logStdMsg("* Switching to new directory... (" + subdirName + ")");

   int renameOldRes = rename(pathOrig.c_str(), pathOldTmp.c_str() );
   if(renameOldRes)
   {
      logErrMsg("Unable to move directory: " + pathOrig + " -> " +
         pathOldTmp + ". SysErr: " + System::getErrString() );

      return false;
   }

   int renameNewRes = rename(pathNewTmp.c_str(), pathOrig.c_str() );
   if(renameNewRes)
   {
      logErrMsg("Unable to move directory: " + pathNewTmp + " -> " +
         pathOrig + ". SysErr: " + System::getErrString() );

      return false;
   }

   logStdMsg("* Cleaning up old directory...");

   for(unsigned i=0; i < CONFIG_HASHDIRS_NUM_OLD; i++)
   {
      std::string subdirNameOld = StringTk::uintToHexStr(i);
      std::string subdirPathOld = pathOldTmp + "/" + subdirNameOld;

      int rmSubRes = rmdir(subdirPathOld.c_str() );
      if(rmSubRes)
      { // error, but not critical
         logErrMsg("Unable to remove directory: " + subdirPathOld + ". " +
            "SysErr: " + System::getErrString() );
      }
   }

   int rmParentRes = rmdir(pathOldTmp.c_str() );
   if(rmParentRes)
   { // error, but not critical
      logErrMsg("Unable to remove directory: " + pathOldTmp + ". " +
         "SysErr: " + System::getErrString() );
   }

   return true;
}

bool createFormatFile(std::string storageDir, bool useXAttr)
{
   std::string filepath = storageDir + "/" + STORAGETK_FORMAT_FILENAME;
   std::string line;

   std::ofstream file(filepath.c_str(),
      std::ios_base::out | std::ios_base::in | std::ios_base::trunc);

   if(!file.is_open() || file.fail() )
   {
      logStdMsg("Failed to open file for writing: " + filepath);
      return false;
   }

   file << "# This file was auto-generated. Do not modify it!" << std::endl;
   file << "version=2" << std::endl;
   file << "xattr=" << (useXAttr ? "true" : "false") << std::endl;

   if(file.fail() )
   {
      file.close();

      logErrMsg("Failed to save data to file: " + filepath);
      return false;
   }

   file.close();

   return true;
}

/**
 * Reads nodeID from file if a nodeID-file exists or uses the hostname.
 *
 * @return false on error
 */
bool readNodeID(std::string storageDir, std::string* outNodeID)
{
   std::string nodeIDPath = storageDir + "/" + CONFIG_NODEID_FILENAME;

   logStdMsg("* Reading NodeID... ", false); // (no std::endl here)

   if(StorageTk::pathExists(nodeIDPath) )
   {
      StringMap nodeIDMap;
      bool loadRes = MapTk::loadStringMapFromFile(nodeIDPath.c_str(), &nodeIDMap);
      if(!loadRes)
      {
         logErrMsg("Unable to load nodeID file: " + nodeIDPath);
         return false;
      }

      if(!nodeIDMap.empty() )
         *outNodeID = nodeIDMap.begin()->first;
   }

   if(outNodeID->empty() )
      *outNodeID = System::getHostname();


   logStdMsg("NodeID: " + *outNodeID);


   return true;
}

void printFinalInfo()
{
   logStdMsg("");
   logStdMsg("* Format conversion complete.");
   logStdMsg("");

   logStdMsg("* Note: This was only the conversion of the metadata directory. You still need "
      "to uninstall the FhGFS 2009.08 packages, install the new packages and set the config "
      "file options. Afterwards, fhgfs-ctl must be used to refresh the metadata information "
      "on all servers.");

   logStdMsg("");
}


int main(int argc, char** argv)
{
   // note: we expect exactly two user arguments: path to the storage directory and xattr/noxattr

   if( (argc != 3) ||
       !strcmp(argv[1], "-h") ||
       !strcmp(argv[1], "--help") ||
       (strcmp(argv[2], "xattr") && strcmp(argv[2], "noxattr") ) )
   {
      char* progPath = argv[0];
      std::string progName = basename(progPath);

      printUsage(progName);
      return 1;
   }

   char* storageDir = argv[1];
   bool useXAttr = !strcmp(argv[2], "xattr");

   std::string nodeID;

   if(!checkAndLockStorageDir(storageDir) )
      return 1;

   if(!readNodeID(storageDir, &nodeID) )
      return 1;

   if(!createStructureTmpDir(storageDir) )
      return 1;

   if(!createEntriesTmpDir(storageDir) )
      return 1;

   if(!walkStructureDirs(storageDir, nodeID, useXAttr) ) // walks and calls migrateStructureSubdir()
      return 1;

   if(!migrateEntryFilesToTmpDir(storageDir, useXAttr) )
      return 1;

   if(!createFormatFile(storageDir, useXAttr) )
      return 1;

   if(!switchDirToNew(storageDir,
      CONFIG_STRUCTURE_SUBDIR_NAME, CONFIG_STRUCTURE_SUBDIR_NAME_NEW_TMP) )
      return 1;

   if(!switchDirToNew(storageDir,
      CONFIG_ENTRIES_SUBDIR_NAME, CONFIG_ENTRIES_SUBDIR_NAME_NEW_TMP) )
      return 1;

   printFinalInfo();

   return 0;
}
