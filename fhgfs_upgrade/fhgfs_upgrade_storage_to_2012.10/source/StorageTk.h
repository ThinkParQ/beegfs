#ifndef STORAGETK_H_
#define STORAGETK_H_

#include "Common.h"
#include "Path.h"


#include <dirent.h>


#define STORAGETK_FILE_MAX_LINE_LENGTH    1024
#define STORAGETK_FILE_COMMENT_CHAR       '#'

#define STORAGETK_ORIGINALNODEID_FILENAME "originalNodeID" /* contains first-run nodeID */
#define STORAGETK_NODEID_FILENAME         "nodeID" /* to force a certain nodeID */
#define STORAGETK_NODENUMID_FILENAME      "nodeNumID" /* contains first-run numeric node ID */

#define STORAGETK_TARGETID_FILENAME       "targetID" /* contains first-run targetID */

#define STORAGETK_TMPFILE_EXT                ".tmp" /* tmp extension for saved files until rename */
#define STORAGETK_TARGETNUMID_FILENAME    "targetNumID" /* contains first-run targetNumID */
#define STORAGETK_DEFAULT_FILEMODE           (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

class StorageTk
{
   public:
      static bool createPathOnDisk(Path& path, bool excludeLastElement);
      static bool removePathDirsFromDisk(Path& path, unsigned keepDepth);

      static bool statStoragePath(const std::string path, int64_t* outSizeTotal,
         int64_t* outSizeFree, int64_t* outInodesTotal, int64_t* outInodesFree);

      static int lockWorkingDirectory(std::string path);
      static void unlockWorkingDirectory(int fd);
      static int createAndLockPIDFile(std::string path, bool writePIDToFile);
      static bool updateLockedPIDFile(int fd);
      static void unlockPIDFile(int fd);

      static bool createStorageFormatFile(const std::string pathStr, int formatVersion,
         StringMap* formatProperties=NULL);
      static bool checkStorageFormatFile(const std::string pathStr, int formatVersion,
         StringMap* outFormatProperties=NULL);
      static bool checkStorageFormatFileExists(const std::string pathStr);


      static bool createNumIDFile(const std::string pathStr, const std::string filename,
         uint16_t targetID);
      static bool readTargetIDFile(const std::string pathStr, std::string* outTargetID);
      static bool createTargetIDFile(const std::string pathStr, const std::string localNodeID);

      static bool readNodeIDFile(const std::string pathStr, std::string fileName,
         std::string& outNodeID);

      static struct dirent* readdirFiltered(DIR* dirp);

      static bool initHashPaths(Path& basePath, int maxLevel1, int maxLevel2);


   private:
      StorageTk() {}

      static unsigned getAndIncIDCounter();

      static bool removePathDirsRec(StringListConstIter dirNameIter, std::string pathStr,
         unsigned currentDepth, unsigned numPathElems, unsigned keepDepth);


   public:
      // inliners
      static bool pathExists(std::string path)
      {
         struct stat statBuf;

         int statRes = stat(path.c_str(), &statBuf);

         return statRes ? false : true;
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

      static void getHashes(const std::string hashStr, size_t numHashesLevel1,
          size_t numHashesLevel2, uint16_t& outHashLevel1, uint16_t& outHashLevel2)
      {
         uint32_t checksum = StringTk::strChecksum(hashStr.c_str(), hashStr.length() );

         outHashLevel1 =  ((uint16_t)(checksum >> 16) ) % numHashesLevel1;
         outHashLevel2 =  ((uint16_t) checksum)         % numHashesLevel2;
      }

};

#endif /*STORAGETK_H_*/
