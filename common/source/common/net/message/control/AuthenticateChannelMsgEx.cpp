#include <common/app/config/ICommonConfig.h>
#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include "AuthenticateChannelMsgEx.h"

bool AuthenticateChannelMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "AuthenticateChannel incoming";

   AbstractApp* app = PThread::getCurrentThreadApp();
   auto cfg = app->getCommonConfig();
   uint64_t authHash = cfg->getConnAuthHash();

   if(authHash && (authHash != getAuthHash() ) )
   { // authentication failed
      LogContext(logContext).log(Log_WARNING,
         "Peer sent wrong authentication: " + ctx.peerName() );
   }
   else
   {
      ctx.getSocket()->setIsAuthenticated();
   }

   // note: this message does not require a response

   return true;
}


