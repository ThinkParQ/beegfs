#ifndef STORAGETK_H_
#define STORAGETK_H_

#include "Common.h"
#include "Path.h"


#include <dirent.h>


#define STORAGETK_FILE_MAX_LINE_LENGTH    1024
#define STORAGETK_FILE_COMMENT_CHAR       '#'
#define STORAGETK_ORIGINALNODEID_FILENAME "originalNodeID" /* contains first-run nodeID */
#define STORAGETK_TARGETID_FILENAME       "targetID" /* contains first-run targetID */

#define STORAGETK_FORMAT_FILENAME         "format.conf"
#define STORAGETK_FORMAT_KEY_VERSION      "version"


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

      static bool createTargetIDFile(const std::string pathStr, const std::string localNodeID);

      static struct dirent* readdirFiltered(DIR* dirp);


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

};

#endif /*STORAGETK_H_*/
