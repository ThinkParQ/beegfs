#include <program/Program.h>
#include <storage/StorageTargets.h>

#include <common/net/message/nodes/GetTargetConsistencyStatesRespMsg.h>

#include "GetTargetConsistencyStatesMsgEx.h"

bool GetTargetConsistencyStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   StorageTargets* storageTargets = Program::getApp()->getStorageTargets();

   const char* logContext = "GetTargetConsistencyStatesMsg incoming";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a GetTargetConsistencyStatesMsg from: "
        + ctx.peerName());
   IGNORE_UNUSED_DEBUG_VARIABLE(logContext);

   auto states = storageTargets->getTargetConsistencyStates(targetIDs);

   ctx.sendResponse(GetTargetConsistencyStatesRespMsg(states));

   return true;
}
