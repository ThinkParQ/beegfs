#include <program/Program.h>
#include "SetChannelDirectMsgEx.h"

bool SetChannelDirectMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "SetChannelDirect incoming";

      LOG_DEBUG(logContext, 4, "Received a SetChannelDirectMsg from: " + ctx.peerName() );

      LOG_DEBUG(logContext, 5, std::string("Value: ") + StringTk::intToStr(getValue() ) );
   #endif // BEEGFS_DEBUG

   ctx.getSocket()->setIsDirect(getValue() );

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_SETCHANNELDIRECT,
      getMsgHeaderUserID() );

   return true;
}


