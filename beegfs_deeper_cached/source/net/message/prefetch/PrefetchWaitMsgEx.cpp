#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/prefetch/PrefetchWaitRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "PrefetchWaitMsgEx.h"



bool PrefetchWaitMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   CacheWorkKey key;
   getKey(key);

   Logger::getLogger()->log(Log_DEBUG, "PrefetchWait", printForLog() );

   int respVal = workMgr->checkSubmittedWork(key, cacheFlags & DEEPER_PREFETCH_SUBDIRS);

   // send response
   ctx.sendResponse(PrefetchWaitRespMsg(respVal) );

   return true;
}
