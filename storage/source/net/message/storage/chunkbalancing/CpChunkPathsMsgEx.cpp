#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/chunkbalancing/CpChunkPathsRespMsg.h>
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>

#include "CpChunkPathsMsgEx.h"

bool CpChunkPathsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "CpChunkPathsMsg incoming"; 
   
   LogContext(logContext).logErr("This message is not yet implemented. \n It should relay chunk information from metadata to storage and trigger copy chunk operation. ");

   FhgfsOpsErr cpMsgRes = FhgfsOpsErr_SUCCESS;
   ctx.sendResponse(CpChunkPathsRespMsg(cpMsgRes));
   return true;
}


ChunkBalancerJob*  CpChunkPathsMsgEx::addChunkBalanceJob()
{
   std::lock_guard<Mutex> mutexLock(ChunkBalanceJobMutex);
      
   ChunkBalancerJob* chunkBalanceJob = nullptr;
   return chunkBalanceJob; 
}