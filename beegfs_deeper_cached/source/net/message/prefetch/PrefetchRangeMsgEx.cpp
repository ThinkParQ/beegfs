#include <common/cache/components/worker/CacheWork.h>
#include <common/cache/net/message/prefetch/PrefetchRangeRespMsg.h>
#include <components/worker/CopyFileRangeWork.h>
#include <components/worker/queue/CacheWorkManager.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "PrefetchRangeMsgEx.h"



bool PrefetchRangeMsgEx::processIncoming(ResponseContext& ctx)
{
   CacheWorkManager* workMgr = Program::getApp()->getWorkQueue();

   Work* work = new CopyFileRangeWork(path, destPath, cacheFlags, CacheWorkType_PREFETCH, startPos,
      numBytes);
   int respVal = workMgr->addWork(work, true);

   // send response
   ctx.sendResponse(PrefetchRangeRespMsg(respVal) );

   return true;
}
