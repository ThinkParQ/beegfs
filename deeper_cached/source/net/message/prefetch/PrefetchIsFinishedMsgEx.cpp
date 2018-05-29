#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/prefetch/PrefetchIsFinishedRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "PrefetchIsFinishedMsgEx.h"



bool PrefetchIsFinishedMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   CacheWorkKey key;
   getKey(key);

   Logger::getLogger()->log(Log_DEBUG, "PrefetchIsFinished", printForLog() );

   int respVal;
   if(workMgr->isWorkQueued(key) )
      respVal = DEEPER_IS_NOT_FINISHED;
   else
      respVal = DEEPER_IS_FINISHED;

   // send response
   ctx.sendResponse(PrefetchIsFinishedRespMsg(respVal) );

   return true;
}
