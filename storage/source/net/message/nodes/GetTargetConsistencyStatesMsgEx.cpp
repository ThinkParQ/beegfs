#include <program/Program.h>
#include <storage/StorageTargets.h>

#include <common/net/message/nodes/GetTargetConsistencyStatesRespMsg.h>

#include "GetTargetConsistencyStatesMsgEx.h"

bool GetTargetConsistencyStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   StorageTargets* storageTargets = Program::getApp()->getStorageTargets();

   TargetConsistencyStateVec states;
   std::transform(
         targetIDs.begin(), targetIDs.end(),
         std::back_inserter(states),
         [storageTargets] (uint16_t targetID) {
            auto* const target = storageTargets->getTarget(targetID);
            return target ? target->getConsistencyState() : TargetConsistencyState_BAD;
         });

   ctx.sendResponse(GetTargetConsistencyStatesRespMsg(states));

   return true;
}
