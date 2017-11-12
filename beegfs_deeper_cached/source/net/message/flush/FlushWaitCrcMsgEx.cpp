#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/flush/FlushWaitCrcRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "FlushWaitCrcMsgEx.h"



bool FlushWaitCrcMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   unsigned long checksum = 0;

   CacheWorkKey key;
   getKey(key);

   Logger::getLogger()->log(Log_DEBUG, "FlushWaitCrc", printForLog() );

   int respVal = workMgr->checkSubmittedWork(key, cacheFlags & DEEPER_FLUSH_SUBDIRS);

   if(respVal == DEEPER_RETVAL_SUCCESS)
   {
      checksum = workMgr->getCrcChecksum(key);
      if(!checksum)
         respVal = ENOENT;
   }

   // send response
   ctx.sendResponse(FlushWaitCrcRespMsg(respVal, checksum) );

   return true;
}
