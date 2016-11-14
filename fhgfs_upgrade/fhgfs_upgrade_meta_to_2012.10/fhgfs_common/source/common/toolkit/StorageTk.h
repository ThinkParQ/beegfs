#ifndef STORAGETK_H_
#define STORAGETK_H_

#include <common/Common.h>
#include <common/app/config/InvalidConfigException.h>
#include <common/storage/Path.h>
#include <common/storage/striping/StripeNodeFileInfo.h>
#include <common/threading/Atomics.h>
#include <common/threading/Mutex.h>
#include <common/threading/SafeMutexLock.h>
#include <common/storage/Storagedata.h>

#include <dirent.h>

#define STORAGETK_FORMAT_FILENAME            "format.conf"

#define STORAGETK_FILE_MAX_LINE_LENGTH    1024
#define STORAGETK_FILE_COMMENT_CHAR       '#'
#define STORAGETK_ORIGINALNODEID_FILENAME "originalNodeID" /* contains first-run nodeID */
#define STORAGETK_NODEID_FILENAME         "nodeID" /* to force a certain nodeID */
#define STORAGETK_NODENUMID_FILENAME      "nodeNumID" /* contains first-run numeric node ID */
#define STORAGETK_TARGETID_FILENAME       "targetID" /* contains first-run targetID */
#define STORAGETK_TARGETNUMID_FILENAME    "targetNumID" /* contains first-run targetNumID */

#define STORAGETK_FILEID_TIMESTAMP_SHIFTBITS  32 /* timestamp is shifted by this number of bits */


class StorageTk
{
   public:
      static void initHashPaths(Path& basePath, int maxLevel1, int maxLevel2);

      static bool createPathOnDisk(Path& path, bool excludeLastElement);
      static bool removePathDirsFromDisk(Path& path, unsigned keepDepth);

      static bool removePathDirContentsFromDisk(Path& path);

      static bool statStoragePath(const std::string path, int64_t* outSizeTotal,
         int64_t* outSizeFree, int64_t* outInodesTotal, int64_t* outInodesFree);
      static bool statStoragePath(Path& path, bool excludeLastElement, int64_t* outSizeTotal,
         int64_t* outSizeFree, int64_t* outInodesTotal, int64_t* outInodesFree);

      static int lockWorkingDirectory(std::string path);
      static void unlockWorkingDirectory(int fd);
      static int createAndLockPIDFile(std::string path, bool writePIDToFile);
      static bool updateLockedPIDFile(int fd);
      static void unlockPIDFile(int fd);

      static bool createStorageFormatFile(const std::string pathStr, int formatVersion,
         StringMap* formatProperties=NULL);
      static bool checkStorageFormatFile(const std::string pathStr, int oldFormatVersion,
         int newFormatVersion, StringMap* outFormatProperties = NULL) throw(InvalidConfigException);
      static bool checkStorageFormatFileExists(const std::string pathStr);

      static void checkOrCreateOrigNodeIDFile(const std::string pathStr, std::string currentNodeID)
         throw(InvalidConfigException);

      static void readTargetIDFile(const std::string pathStr, std::string* outTargetID)
         throw(InvalidConfigException);
      static void readOrCreateTargetIDFile(const std::string pathStr, uint16_t localNodeID,
         std::string* outTargetID) throw(InvalidConfigException);
      static void readNumIDFile(const std::string pathStr,  const std::string filename,
         uint16_t* outTargetNumID) throw(InvalidConfigException);
      static void createNumIDFile(const std::string pathStr, const std::string filename,
         uint16_t targetID) throw(InvalidConfigException);

      static struct dirent* readdirFiltered(DIR* dirp);
      static void readCompleteDir(const char* path, StringList* outNames)
         throw(InvalidConfigException);

      static ssize_t readAIOSync(int fd, void* buf, size_t count, int64_t offset);
      static ssize_t writeAIOSync(int fd, void* buf, size_t count, int64_t offset);

      static void updateDynamicFileInodeAttribs(StripeNodeFileInfoVec& fileInfoVec,
         StripePattern* stripePattern, StatData& outStatData);

   private:
      StorageTk() {}

      static AtomicUInt64 idCounter; // high 32bit are timestamp, low 32bits are sequential counter


      static bool removePathDirsRec(StringListConstIter dirNameIter, std::string pathStr,
         unsigned currentDepth, unsigned numPathElems, unsigned keepDepth);


   public:
      // inliners

      /**
       * Check if given path exists.
       */
      static bool pathExists(std::string path)
      {
         struct stat statBuf;

         int statRes = stat(path.c_str(), &statBuf);

         return statRes ? false : true;
      }

      /**
       * Generate ID for new fs entry (i.e. file or dir).
       *
       * Note: Currently also used for targetIDs.
       */
       static std::string generateFileID(uint16_t localNodeID)
      {
         /* note: we assume here that the clock doesn't jump backwards between restarts of
            the daemon (and that there always is at least one second between restarts) and that we
            don't need more than 2^32 IDs per second (sustained) */

         uint64_t nextID = idCounter.increase();

         // note on idCounter value: high 32bit are timestamp, low 32bits are sequential counter

         /* note on switching high/low: having the timestamp first is bad for strcmp() and such
           things, which the underlying fs might need to do - because in that order, that the first
           characters of entryIDs/filenames would be similar. */

         uint32_t counterPart = (uint32_t)nextID;
         uint32_t timestampPart = (uint32_t)(nextID >> STORAGETK_FILEID_TIMESTAMP_SHIFTBITS);

         return StringTk::uintToHexStr(counterPart) + "-" +
            StringTk::uintToHexStr(timestampPart) + "-" +
            StringTk::uintToHexStr(localNodeID);
      }

      /**
       * According to linux-src/include/linux/fs.h we can convert from the file mode as provided in
       * 'struct stat' by the field 'st_mode' to the file type as given by
       * 'struct dirent' in the field 'd_type' by some bit magic operations
       * FIXME: Put it somewhere into fhgfs-common
       */
      static int modeToDentryType(int mode)
      {
         int dentryType = (mode >> 12) & 15;

         return dentryType;
      }

      static void getHashes(const std::string hashStr, size_t numHashesLevel1,
          size_t numHashesLevel2, uint16_t& outHashLevel1, uint16_t& outHashLevel2)
      {
         uint32_t checksum = StringTk::strChecksum(hashStr.c_str(), hashStr.length() );

         outHashLevel1 =  ((uint16_t)(checksum >> 16) ) % numHashesLevel1;
         outHashLevel2 =  ((uint16_t) checksum)         % numHashesLevel2;
      }


      static unsigned getChunkHash(const std::string id,
         size_t numHashesLevel1, size_t numHashesLevel2)
      {
         uint16_t hashLevel1;
         uint16_t hashLevel2;

         getHashes(id, numHashesLevel1, numHashesLevel2, hashLevel1, hashLevel2);

         unsigned outHash = ( (unsigned (hashLevel1)) << 16) + hashLevel2;

         return outHash;
      }

      /**
       * Get complete path to fileName for chunk files.
       *
       * Note: computes hash based on fileName.
       *
       * @param targetPath the path of a storage target
       * @return targetPath/chunksDir/hashDir1/hashDir2/fileName
       */
      static std::string getFileChunkPathV2(const std::string targetPath,
         const std::string fileName)
      {
         return getHashPath(targetPath + "/" CONFIG_CHUNK_SUBDIR_NAME, fileName,
            CONFIG_CHUNK_LEVEL1_SUBDIR_NUM, CONFIG_CHUNK_LEVEL2_SUBDIR_NUM);
      }

      /**
       * Get complete path to fileName for mirrored chunk files.
       *
       * Note: computes hash based on fileName.
       *
       * @param targetPath the path of a storage target
       * @return targetPath/targetID.chunksDir/hashDir1/hashDir2/fileName
       */
      static std::string getMirroredFileChunkPath(const std::string targetPath,
         const std::string fileName, uint16_t mirroredFromTargetID)
      {
         return getHashPath(targetPath + "/" +
            StringTk::uintToStr(mirroredFromTargetID) + "." CONFIG_CHUNK_SUBDIR_NAME, fileName,
            CONFIG_CHUNK_LEVEL1_SUBDIR_NUM, CONFIG_CHUNK_LEVEL2_SUBDIR_NUM);
      }

      /**
       * @return path/hashDir1/hashDir2/fileName
       */
      static std::string getHashPath(const std::string path, const std::string fileName,
         size_t numHashesLevel1, size_t numHashesLevel2)
      {
         uint16_t hashLevel1;
         uint16_t hashLevel2;

         getHashes(fileName, numHashesLevel1, numHashesLevel2, hashLevel1, hashLevel2);

         return path + "/" +
            StringTk::uint16ToHexStr(hashLevel1) + "/" + StringTk::uint16ToHexStr(hashLevel2) +
            "/" + fileName;
      }

      static void splitHashDirs(unsigned hashDirs, unsigned* firstLevelHashDirOut,
         unsigned* secondLevelHashDirOut)
      {
         // from the incoming hash dir, we create a first level hash dir and a second level hash
         // dir, by splitting the 32-bit integer into two halfs
         *firstLevelHashDirOut = hashDirs >> 16;
         *secondLevelHashDirOut = hashDirs & 65535; // cut of at 16-bit
      }

      static unsigned mergeHashDirs(unsigned firstLevelhashDirNum, unsigned secondLevelHashDirNum)
      {
         // 32 bit integer; first 16 bit for first level hash, second 16 bit for second level
         // this is enough space for the currently used hash dirs
         unsigned hashDirNum = firstLevelhashDirNum << 16; // add first level
         hashDirNum = hashDirNum + secondLevelHashDirNum; // add second level

         return hashDirNum;
      }
};

#endif /*STORAGETK_H_*/
