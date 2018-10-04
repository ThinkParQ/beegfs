#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/nodes/TargetStateStore.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>

#include "SetTargetConsistencyStatesMsgEx.h"

bool SetTargetConsistencyStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   StorageTargets* storageTargets = app->getStorageTargets();
   FhgfsOpsErr result = FhgfsOpsErr_SUCCESS;

   if (getTargetIDs().size() != getStates().size())
   {
      LogContext(__func__).logErr("Different list size of targetIDs and states");
      result = FhgfsOpsErr_INTERNAL;
      goto send_response;
   }

   for (ZipIterRange<UInt16List, UInt8List> idStateIter(getTargetIDs(), getStates());
        !idStateIter.empty(); ++idStateIter)
   {
      auto* const target = storageTargets->getTarget(*idStateIter()->first);
      if (!target)
      {
         LogContext(__func__).logErr("Unknown targetID: " +
            StringTk::uintToStr(*(idStateIter()->first) ) );
         result = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }

      target->setState(TargetConsistencyState(*idStateIter()->second));
   }

send_response:
   ctx.sendResponse(SetTargetConsistencyStatesRespMsg(result) );

   return true;
}
