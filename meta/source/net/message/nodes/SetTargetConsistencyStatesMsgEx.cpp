#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/nodes/TargetStateStore.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>

#include "SetTargetConsistencyStatesMsgEx.h"

bool SetTargetConsistencyStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "Set target states incoming";

   App* app = Program::getApp();
   InternodeSyncer* internodeSyncer = app->getInternodeSyncer();
   FhgfsOpsErr result = FhgfsOpsErr_SUCCESS;

   if (getTargetIDs().size() != 1 || getStates().size() != 1)
   {
      LogContext(logContext).logErr("Invalid list size of target ID or state List (must be 1).");
      result = FhgfsOpsErr_INTERNAL;
      goto send_response;
   }

   if (getTargetIDs().front() != app->getLocalNodeNumID().val() )
   {
      LogContext(logContext).logErr("ID in message does not match local node ID.");
      result = FhgfsOpsErr_INTERNAL;
      goto send_response;
   }

   internodeSyncer->setNodeConsistencyState(
      static_cast<TargetConsistencyState>(getStates().front() ) );

send_response:
   ctx.sendResponse(SetTargetConsistencyStatesRespMsg(result) );

   return true;
}
