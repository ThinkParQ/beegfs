#include <program/Program.h>
#include "DummyMsgEx.h"

bool DummyMsgEx::processIncoming(ResponseContext& ctx)
{
   ctx.sendResponse(DummyMsg() );

   return true;
}
