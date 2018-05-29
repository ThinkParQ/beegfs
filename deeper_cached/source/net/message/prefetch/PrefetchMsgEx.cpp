#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/prefetch/PrefetchRespMsg.h>
#include <components/worker/CopyFileWork.h>
#include <components/worker/ProcessDirWork.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "PrefetchMsgEx.h"



bool PrefetchMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   Logger* log = Logger::getLogger();
   CacheWorkManager* workMgr = app->getWorkQueue();

   Work* work;
   int respVal;

   if(cacheFlags & DEEPER_PREFETCH_SUBDIRS)
   {
      struct stat statSource;
      int funcError = stat(path.c_str(), &statSource);
      if(funcError != 0)
      {
         log->log(Log_ERR, __FUNCTION__, "Could not stat source file/directory: " + path +
            "; errno: " + System::getErrString(errno) );
         respVal = DEEPER_RETVAL_ERROR;
      }
      else if(S_ISDIR(statSource.st_mode) )
      {
         Work* work = new ProcessDirWork(path, destPath, cacheFlags, CacheWorkType_PREFETCH);
         respVal = workMgr->addWork(work, false);
      }
      else
      {
         Work* work = new CopyFileWork(path, destPath, cacheFlags & (~DEEPER_PREFETCH_SUBDIRS),
            CacheWorkType_PREFETCH);
         respVal = workMgr->addWork(work, false);
      }
   }
   else
   {
      work = new CopyFileWork(path, destPath, cacheFlags, CacheWorkType_PREFETCH);
      respVal = workMgr->addWork(work, false);
   }

   // send response
   ctx.sendResponse(PrefetchRespMsg(respVal) );

   return true;
}
