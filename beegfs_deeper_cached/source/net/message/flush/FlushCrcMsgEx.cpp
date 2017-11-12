#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/flush/FlushCrcRespMsg.h>
#include <components/worker/CopyFileCrcWork.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "FlushCrcMsgEx.h"



bool FlushCrcMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   Work* work = new CopyFileCrcWork(path, destPath, cacheFlags, CacheWorkType_FLUSH);
   int respVal = workMgr->addWork(work, false);

   // send response
   ctx.sendResponse(FlushCrcRespMsg(respVal) );

   return true;
}
