#include <common/app/config/ICommonConfig.h>
#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include "PeerInfoMsgEx.h"

bool PeerInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   ctx.getSocket()->setNodeType(type);
   ctx.getSocket()->setNodeID(id);

   // note: this message does not require a response

   return true;
}


