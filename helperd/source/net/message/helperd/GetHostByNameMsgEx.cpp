#include <program/Program.h>
#include <common/net/message/helperd/GetHostByNameRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SocketTk.h>
#include "GetHostByNameMsgEx.h"


bool GetHostByNameMsgEx::processIncoming(ResponseContext& ctx)
{
   // note: this will send an empty string in the response when the system was unable to
   //    resolve the hostname

   const std::string hostname = getHostname();
   std::string hostAddrStr;

   const auto hostIP = SocketTk::getHostByName(hostname);
   if(hostIP)
      hostAddrStr = Socket::ipaddrToStr(hostIP.value());
   else
      LOG(COMMUNICATION, ERR, "Failed to resolve hostname.", hostname,
            ("System error", hostIP.error().message()));

   ctx.sendResponse(GetHostByNameRespMsg(hostAddrStr.c_str() ) );

   return true;
}

