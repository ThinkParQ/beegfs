#include <app/App.h>
#include <app/config/Config.h>
#include <common/app/log/Logger.h>
#include <common/cache/toolkit/cachepathtk.h>
#include <common/cache/toolkit/filesystemtk.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include <toolkit/asyncfilesystemtk.h>

#include "CopyFileRangeWork.h"



void CopyFileRangeWork::doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   Logger* log = Logger::getLogger();

   int retVal;
   bool retValPath = true;
   if(!splitRootWork)
   {
      if(this->workType == CacheWorkType_FLUSH)
      {
         retValPath = cachepathtk::createPath(destPath, cfg->getSysMountPointCache(),
            cfg->getSysMountPointGlobal(), true, log);
      }
      else
      {
         retValPath = cachepathtk::createPath(destPath, cfg->getSysMountPointGlobal(),
            cfg->getSysMountPointCache(), true, log);
      }
   }

   if(!retValPath)
   {
      this->updateResult(errno);
   }
   else
   {
      uint64_t splitBlockSize = cfg->getTuneFileSplitSize();
      off_t startOffset = startPosition;

      int numWork = asyncfilesystemtk::calculateNumCopyThreads(splitBlockSize, numBytes);
      uint64_t lastSplitBlockSize = (numBytes - (splitBlockSize * (numWork -1) ) );
      if(numWork > 1)
      {
         do
         {
#ifdef BEEGFS_DEBUG
            Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
               "Copy file path: " + sourcePath +
               "; num splits: " + StringTk::intToStr(numWork) +
               "; startByte: " + StringTk::uint64ToStr(startOffset) );
#endif
            Work* work = new CopyFileRangeWork(sourcePath, destPath, deeperFlags, workType,
               NULL, splitRootWork ? splitRootWork : this, numFollowedSymlinks, startOffset,
               splitBlockSize);

            int addRetVal = app->getWorkQueue()->addSplitWork(work);
            if(addRetVal != DEEPER_RETVAL_SUCCESS)
               updateResult(addRetVal);

            startOffset += splitBlockSize;
            numWork--;
         } while (numWork > 1);

#ifdef BEEGFS_DEBUG
            Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
               "Copy file path: " + sourcePath +
               "; num splits: " + StringTk::intToStr(numWork) +
               "; startByte: " + StringTk::uint64ToStr(startOffset) );
#endif
         retVal = filesystemtk::copyFileRange(sourcePath.c_str(), destPath.c_str(), NULL,
            &startOffset, lastSplitBlockSize, splitRootWork ? splitRootWork : this,
            splitRootWork ? false : true);
      }
      else
      {
         retVal = filesystemtk::copyFileRange(sourcePath.c_str(), destPath.c_str(), NULL,
            &startPosition, numBytes, splitRootWork ? splitRootWork : this,
            splitRootWork ? false : true);
      }

      if(retVal != DEEPER_RETVAL_SUCCESS)
         updateResult(errno);
   }
}
