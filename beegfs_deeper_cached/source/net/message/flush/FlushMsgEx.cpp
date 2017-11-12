#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/flush/FlushRespMsg.h>
#include <components/worker/CopyFileWork.h>
#include <components/worker/ProcessDirWork.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "FlushMsgEx.h"



bool FlushMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   Logger* log = Logger::getLogger();
   CacheWorkManager* workMgr = app->getWorkQueue();

   Work* work;
   int respVal;

   if(cacheFlags & DEEPER_FLUSH_SUBDIRS)
   {
      struct stat statSource;
      int funcError = stat(path.c_str(), &statSource);
      if(funcError != 0)
      {
         log->log(Log_ERR, __FUNCTION__, "Could not stat source file/directory: " + path +
            "; errno: " + System::getErrString(errno) );
         respVal = DEEPER_RETVAL_ERROR;
      }
      else if(S_ISDIR(statSource.st_mode))
      {
         Work* work = new ProcessDirWork(path, destPath, cacheFlags, CacheWorkType_FLUSH);
         respVal = workMgr->addWork(work, false);
      }
      else
      {
         Work* work = new CopyFileWork(path, destPath, cacheFlags & (~DEEPER_FLUSH_SUBDIRS),
            CacheWorkType_FLUSH);
         respVal = workMgr->addWork(work, false);
      }
   }
   else
   {
      work = new CopyFileWork(path, destPath, cacheFlags, CacheWorkType_FLUSH);
      respVal = workMgr->addWork(work, false);
   }

   // send response
   ctx.sendResponse(FlushRespMsg(respVal) );

   return true;
}
