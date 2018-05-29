#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/prefetch/PrefetchStopRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "PrefetchStopMsgEx.h"



bool PrefetchStopMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   Logger::getLogger()->log(Log_DEBUG, "PrefetchStop", printForLog() );

   CacheWorkKey key;
   getKey(key);

   bool success = workMgr->stopWork(key);

   // send response
   ctx.sendResponse(PrefetchStopRespMsg(success ? DEEPER_RETVAL_SUCCESS : ENOENT) );

   return true;
}
