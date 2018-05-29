#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/prefetch/PrefetchWaitCrcRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "PrefetchWaitCrcMsgEx.h"



bool PrefetchWaitCrcMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   CacheWorkKey key;
   getKey(key);

   Logger::getLogger()->log(Log_DEBUG, "PrefetchWaitCrc", printForLog() );

   unsigned long checksum = 0;

   int respVal = workMgr->checkSubmittedWork(key, cacheFlags & DEEPER_PREFETCH_SUBDIRS);
   if(respVal == DEEPER_RETVAL_SUCCESS)
   {
      checksum = workMgr->getCrcChecksum(key);
      if(!checksum)
         respVal = ENOENT;
   }

   // send response
   ctx.sendResponse(PrefetchWaitCrcRespMsg(respVal, checksum) );

   return true;
}
