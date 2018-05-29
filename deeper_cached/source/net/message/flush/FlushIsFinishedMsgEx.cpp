#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/flush/FlushIsFinishedRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "FlushIsFinishedMsgEx.h"



bool FlushIsFinishedMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   CacheWorkKey key;
   getKey(key);

   Logger::getLogger()->log(Log_DEBUG, "FlushIsFinished", printForLog() );

   int respVal;
   if(workMgr->isWorkQueued(key) )
      respVal = DEEPER_IS_NOT_FINISHED;
   else
      respVal = DEEPER_IS_FINISHED;

   // send response
   ctx.sendResponse(FlushIsFinishedRespMsg(respVal) );

   return true;
}
