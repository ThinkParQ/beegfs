#include <program/Program.h>
#include <common/net/message/helperd/GetHostByNameRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SocketTk.h>
#include "GetHostByNameMsgEx.h"


bool GetHostByNameMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetHostByNameMsgEx incoming");
   LOG_DEBUG_CONTEXT(log, 4, "Received a GetHostByNameMsg from: " + ctx.peerName() );

   // note: this will send an empty string in the response when the system was unable to
   //    resolve the hostname

   const char* hostname = getHostname();
   struct in_addr hostIP;
   std::string hostAddrStr;

   bool resolveRes = SocketTk::getHostByName(hostname, &hostIP);
   if(resolveRes)
      hostAddrStr = Socket::ipaddrToStr(&hostIP);

   ctx.sendResponse(GetHostByNameRespMsg(hostAddrStr.c_str() ) );

   return true;
}

