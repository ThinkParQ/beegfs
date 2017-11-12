#include <common/app/config/ICommonConfig.h>
#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include "AuthenticateChannelMsgEx.h"

bool AuthenticateChannelMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "AuthenticateChannel incoming";

   LOG_DEBUG(logContext, Log_DEBUG,
      "Received a AuthenticateChannelMsg from: " + ctx.peerName() );

   AbstractApp* app = PThread::getCurrentThreadApp();
   ICommonConfig* cfg = app->getCommonConfig();
   uint64_t authHash = cfg->getConnAuthHash();

   if(authHash && (authHash != getAuthHash() ) )
   { // authentication failed

      #ifdef BEEGFS_DEBUG
         // print more info in debug mode
         LOG_DEBUG(logContext, Log_WARNING,
            "Peer sent wrong authentication: " + ctx.peerName() + " "
               "My authHash: " + StringTk::uint64ToHexStr(getAuthHash() ) + " "
               "Peer authHash: " + StringTk::uint64ToHexStr(getAuthHash() ) );
      #else
         LogContext(logContext).log(Log_WARNING,
            "Peer sent wrong authentication: " + ctx.peerName() );
      #endif // BEEGFS_DEBUG
   }
   else
   {
      LOG_DEBUG(logContext, Log_SPAM,
         "Authentication successful. "
            "My authHash: " + StringTk::uint64ToHexStr(getAuthHash() ) + " "
            "Peer authHash: " + StringTk::uint64ToHexStr(getAuthHash() ) );

      ctx.getSocket()->setIsAuthenticated();
   }

   // note: this message does not require a response

   return true;
}


