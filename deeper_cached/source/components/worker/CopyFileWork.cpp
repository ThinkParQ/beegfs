#include <app/config/Config.h>
#include <common/app/log/Logger.h>
#include <common/cache/toolkit/cachepathtk.h>
#include <common/cache/toolkit/filesystemtk.h>
#include <common/cache/toolkit/threadvartk.h>
#include <components/worker/CopyFileRangeWork.h>
#include <components/worker/ProcessDirWork.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include <toolkit/asyncfilesystemtk.h>
#include "CopyFileWork.h"



void CopyFileWork::doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   Logger* log = Logger::getLogger();

   int funcError;
   int retVal;
   struct stat statSource;
   bool doDiscard;
   bool doFollowSymlink;

   bool retValPath;
   bool isPrefetch = this->workType == CacheWorkType_PREFETCH;
   if(isPrefetch)
   {
      retValPath = cachepathtk::createPath(destPath, cfg->getSysMountPointGlobal(),
         cfg->getSysMountPointCache(), true, log);
      doDiscard = false;
      doFollowSymlink = deeperFlags & DEEPER_PREFETCH_FOLLOWSYMLINKS;
   }
   else
   {
      retValPath = cachepathtk::createPath(destPath, cfg->getSysMountPointCache(),
         cfg->getSysMountPointGlobal(), true, log);
      doDiscard = deeperFlags & DEEPER_FLUSH_DISCARD;
      doFollowSymlink = deeperFlags & DEEPER_FLUSH_FOLLOWSYMLINKS;
   }

   if(!retValPath)
   {
      retVal = DEEPER_RETVAL_ERROR;
      goto set_result;
   }

   funcError = lstat(sourcePath.c_str(), &statSource);
   if(funcError != 0)
   {
      log->log(Log_ERR, __FUNCTION__, "Could not stat source file/directory " + sourcePath +
         "; errno: " + System::getErrString(errno) );
      retVal = DEEPER_RETVAL_ERROR;
      goto set_result;
   }

   if(S_ISLNK(statSource.st_mode) )
   {
      if(doFollowSymlink)
      {
         retVal = asyncfilesystemtk::handleFollowSymlink(sourcePath, destPath, &statSource,
            isPrefetch, false, true, false, deeperFlags, workType, rootWork ? rootWork : this,
            numFollowedSymlinks, NULL);
      }
      else
      {
         retVal = filesystemtk::createSymlink(sourcePath.c_str(), destPath.c_str(), &statSource,
            isPrefetch, doDiscard);
      }
   }
   else if(S_ISREG(statSource.st_mode) )
   {
      uint64_t splitBlockSize = cfg->getTuneFileSplitSize();
      off_t startOffset = 0;

      int numWork = asyncfilesystemtk::calculateNumCopyThreads(splitBlockSize, statSource.st_size);
      uint64_t lastSplitBlockSize = (statSource.st_size - (splitBlockSize * (numWork -1) ) );

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

            Work* work = new CopyFileRangeWork(sourcePath, destPath, deeperFlags, workType, NULL,
               this, numFollowedSymlinks, startOffset, splitBlockSize);

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
            &startOffset, lastSplitBlockSize, this, true);

         if(doDiscard)
            setDoDiscardForSplit();
      }
      else
      {
         retVal = filesystemtk::copyFile(sourcePath.c_str(), destPath.c_str(), &statSource,
            doDiscard, false, this, NULL);
      }
   }
   else if(S_ISDIR(statSource.st_mode) )
   {
      errno = EISDIR;
      log->log(Log_ERR, __FUNCTION__,
         std::string("Given path is an directory but DEEPER_*_SUBDIRS flag is not set: ") +
         sourcePath);
      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   {
      errno = EINVAL;
      log->log(Log_ERR, __FUNCTION__, std::string("Unsupported file type for path: ") + sourcePath);
      retVal = DEEPER_RETVAL_ERROR;
   }


set_result:
   if(retVal != DEEPER_RETVAL_SUCCESS)
      updateResult(errno);
}
