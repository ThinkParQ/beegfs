#include <app/config/Config.h>
#include <common/app/log/Logger.h>
#include <common/cache/toolkit/cachepathtk.h>
#include <common/cache/toolkit/filesystemtk.h>
#include <common/cache/toolkit/threadvartk.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include <toolkit/asyncfilesystemtk.h>
#include "CopyFileCrcWork.h"



void CopyFileCrcWork::doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   Config* cfg = Program::getApp()->getConfig();
   Logger* log = Logger::getLogger();

   int funcError;
   int retVal;
   struct stat statSource;
   unsigned long crcValue = 0;
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
      log->log(Log_ERR, __FUNCTION__, "Could not stat source file/directory: " + sourcePath +
         "; errno: " + System::getErrString(errno) );
      retVal = DEEPER_RETVAL_ERROR;
      goto set_result;
   }

   if(S_ISREG(statSource.st_mode) )
   {
      retVal = filesystemtk::copyFile(sourcePath.c_str(), destPath.c_str(), &statSource, doDiscard,
         true, this, &crcValue);
   }
   else if(S_ISLNK(statSource.st_mode) )
   {
      if(doFollowSymlink)
      {
         retVal = asyncfilesystemtk::handleFollowSymlink(sourcePath, destPath.c_str(), &statSource,
            isPrefetch, doDiscard, true, true, deeperFlags, workType, rootWork ? rootWork : this,
            numFollowedSymlinks, &crcValue);
      }
      else
      {
         errno = ENOLINK;
         log->log(Log_ERR, __FUNCTION__, std::string("Given path is a symlink. "
            "CRC checksum calculation requires a file or the DEEPER_*_FOLLOWSYMLINKS flag: ") +
            sourcePath);
         retVal = DEEPER_RETVAL_ERROR;
      }
   }
   else if(S_ISDIR(statSource.st_mode) )
   {
      errno = EISDIR;
      log->log(Log_ERR, __FUNCTION__, std::string("Given path is an directory."
         "CRC checksum calculation doesn't support directories: ") + sourcePath);
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
   {
      updateResult(errno);
   }
   else
   {
      CacheWorkKey key;

      if(rootWork)
      { // in case of follow symlink the CRC checksum calculation is done by a child but the
        // CRC checksum is requested by the original path which is stored in the root work
         rootWork->getKey(key);
         Program::getApp()->getWorkQueue()->addCrcChecksum(key, crcValue);
      }
      else
      {
         getKey(key);
         Program::getApp()->getWorkQueue()->addCrcChecksum(key, crcValue);
      }
   }
}
