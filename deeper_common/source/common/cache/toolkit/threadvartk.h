#ifndef COMMON_CACHE_TOOLKIT_THREADVARTK_H_
#define COMMON_CACHE_TOOLKIT_THREADVARTK_H_


#include <pthread.h>
#include <string>



namespace threadvartk
{
   void makeThreadKeyNftwFlushDiscard();
   void freeThreadKeyNftwFlushDiscard(void* value);
   void setThreadKeyNftwFlushDiscard(bool flushFlag);
   bool getThreadKeyNftwFlushDiscard();

   void makeThreadKeyNftwFollowSymlink();
   void freeThreadKeyNftwFollowSymlink(void* value);
   void setThreadKeyNftwFollowSymlink(bool followSymlinkFlag);
   bool getThreadKeyNftwFollowSymlink();

   void makeThreadKeyNftwNumFollowedSymlinks();
   void freeThreadKeyNftwNumFollowedSymlinks(void* value);
   void setThreadKeyNftwNumFollowedSymlinks(int numFollowedSymlinks);
   int getThreadKeyNftwNumFollowedSymlinks();
   void resetThreadKeyNftwNumFollowedSymlinks();
   bool testAndIncrementThreadKeyNftwNumFollowedSymlinks();

   void makeThreadKeyNftwDestPath();
   void freeThreadKeyNftwDestPath(void* value);
   void setThreadKeyNftwDestPath(const char* destPath);
   bool getThreadKeyNftwDestPath(std::string& outValue);
   void resetThreadKeyNftwDestPath();

   void makeThreadKeyNftwSourcePath();
   void freeThreadKeyNftwSourcePath(void* value);
   void setThreadKeyNftwSourcePath(const char* sourcePath);
   bool getThreadKeyNftwSourcePath(std::string& outValue);
   void resetThreadKeyNftwSourcePath();

   void setThreadKeyNftwPaths(const char* sourcePath, const char* destPath);
   bool getThreadKeyNftwPaths(std::string& sourcePath, std::string& destPath);
   void resetThreadKeyNftwPaths();
} /* namespace ThreadVarTk */

#endif /* COMMON_CACHE_TOOLKIT_THREADVARTK_H_ */
