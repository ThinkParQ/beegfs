#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/flush/FlushWaitRespMsg.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "FlushWaitMsgEx.h"



bool FlushWaitMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   CacheWorkKey key;
   getKey(key);

   Logger::getLogger()->log(Log_DEBUG, "FlushWait", printForLog() );

   int respVal = workMgr->checkSubmittedWork(key, cacheFlags & DEEPER_FLUSH_SUBDIRS);

   // send response
   ctx.sendResponse(FlushWaitRespMsg(respVal) );

   return true;
}
