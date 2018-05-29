#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/prefetch/PrefetchCrcRespMsg.h>
#include <components/worker/CopyFileCrcWork.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "PrefetchCrcMsgEx.h"



bool PrefetchCrcMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   Work* work = new CopyFileCrcWork(path, destPath, cacheFlags, CacheWorkType_PREFETCH);
   int respVal = workMgr->addWork(work, false);

   // send response
   ctx.sendResponse(PrefetchCrcRespMsg(respVal) );

   return true;
}
