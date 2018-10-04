#include <program/Program.h>
#include "SetChannelDirectMsgEx.h"

bool SetChannelDirectMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("SetChannelDirect incoming");

   LOG_DEBUG_CONTEXT(log, 5, std::string("Value: ") + StringTk::intToStr(getValue() ) );

   ctx.getSocket()->setIsDirect(getValue() );

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      StorageOpCounter_SETCHANNELDIRECT, getMsgHeaderUserID() );

   return true;
}


