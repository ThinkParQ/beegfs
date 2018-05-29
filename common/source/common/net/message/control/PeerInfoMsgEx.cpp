#include <common/app/config/ICommonConfig.h>
#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include "PeerInfoMsgEx.h"

bool PeerInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "PeerInfo incoming";

   (void) logContext;
   LOG_DEBUG(logContext, Log_DEBUG,
      "Received a PeerInfoMsg from: " + ctx.peerName() );

   ctx.getSocket()->setNodeType(type);
   ctx.getSocket()->setNodeID(id);

   // note: this message does not require a response

   return true;
}


