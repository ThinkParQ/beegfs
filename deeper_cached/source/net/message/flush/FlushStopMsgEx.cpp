#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/flush/FlushStopRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "FlushStopMsgEx.h"



bool FlushStopMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   CacheWorkKey key;
   getKey(key);

   Logger::getLogger()->log(Log_DEBUG, "FlushStop", printForLog() );

   bool success = workMgr->stopWork(key);

   // send response
   ctx.sendResponse(FlushStopRespMsg(success ? DEEPER_RETVAL_SUCCESS : ENOENT) );

   return true;
}
