#ifndef TOOLKIT_STRINGPATHTK_H_
#define TOOLKIT_STRINGPATHTK_H_


#include <app/config/Config.h>
#include <common/app/log/Logger.h>
#include <common/Common.h>


class CachePathTk
{
   public:
      static bool isRootPath(const std::string& path, const std::string& rootPath);
      static bool isGlobalPath(const std::string& path, const Config* deeperLibConfig);
      static bool isCachePath(const std::string& path, const Config* deeperLibConfig);
      static bool globalPathToCachePath(std::string& inOutPath, const Config* deeperLibConfig,
         Logger* log);
      static bool cachePathToGlobalPath(std::string& inOutPath, const Config* deeperLibConfig,
         Logger* log);

      static bool makePathAbsolute(std::string& inOutPath, Logger* log);
      static bool makePathAbsolute(std::string& inOutPath, const std::string& startDirOfPath,
         Logger* log);

      static bool createPath(std::string destPath, std::string sourceMount,
         std::string destMount, bool setModeFromSource, Logger* log);
      static bool createPath(const char* destPath, const std::string& sourceMount,
         const std::string& destMount, bool excludeLastElement, Logger* log);
      static bool replaceRootPath(std::string& inOutPath, const std::string& origPath,
         const std::string& newPath, bool canonicalizePaths, Logger* log);

      static bool readLink(const char* sourcePath, const struct stat* statSource,
         std::string& outLinkContent, Logger* log);

      static void preparePaths(std::string& inOutString);


   private:
      CachePathTk();

      static bool bothCharactersAreSlashes(char a, char b);
};

#endif /* TOOLKIT_STRINGPATHTK_H_ */
