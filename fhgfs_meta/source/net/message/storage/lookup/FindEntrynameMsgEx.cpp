#include "FindEntrynameMsgEx.h"

#include <program/Program.h>

bool FindEntrynameMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "FindEntrynameMsg incoming";

   LOG_DEBUG(logContext, 4, "Received a FindEntrynameMsg from: " + ctx.peerName() );

   LogContext(logContext).log(Log_WARNING, "Message handler not implemented");

   return false;
}

