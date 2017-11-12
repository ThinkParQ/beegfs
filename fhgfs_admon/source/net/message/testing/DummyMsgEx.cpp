#include <program/Program.h>
#include "DummyMsgEx.h"

bool DummyMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("DummyMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a DummyMsg from: " + ctx.peerName() );

   ctx.sendResponse(DummyMsg() );

   return true;
}
