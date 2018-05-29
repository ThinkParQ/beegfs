#include <app/App.h>
#include <common/app/log/Logger.h>
#include <deeper/deeper_cache.h>
#include <toolkit/asyncfilesystemtk.h>
#include <program/Program.h>
#include "FollowSymlinkWork.h"



void FollowSymlinkWork::doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   Logger* log = Logger::getLogger();

   int retVal;
   struct stat statSource;

   retVal = lstat(sourcePath.c_str(), &statSource);
   if(retVal != DEEPER_RETVAL_SUCCESS)
   {
      log->log(Log_ERR, __FUNCTION__, "Could not stat source file/directory " + sourcePath +
         "; errno: " + System::getErrString(errno) );
      retVal = DEEPER_RETVAL_ERROR;
   }
   else
   {
      bool doDiscard;
      bool ignoreDir;
      bool isPrefetch = (workType == CacheWorkType_PREFETCH);
      if(isPrefetch)
      {
         ignoreDir = !(deeperFlags & DEEPER_PREFETCH_SUBDIRS);
         doDiscard = false;
      }
      else
      {
         ignoreDir = !(deeperFlags & DEEPER_FLUSH_SUBDIRS);
         doDiscard = ignoreDir ? (deeperFlags & DEEPER_FLUSH_DISCARD) : false;
      }

      retVal = asyncfilesystemtk::handleFollowSymlink(sourcePath.c_str(), destPath.c_str(),
         &statSource, isPrefetch, doDiscard, ignoreDir, false, deeperFlags, workType,
         rootWork ? rootWork : this, numFollowedSymlinks, NULL);
   }

   if(retVal != DEEPER_RETVAL_SUCCESS)
      updateResult(errno);
}
