// control messages
#include <common/net/message/control/AuthenticateChannelMsgEx.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/control/PeerInfoMsgEx.h>
#include "control/SetChannelDirectMsgEx.h"
// helperd messages
#include "helperd/GetHostByNameMsgEx.h"
#include <common/net/message/helperd/GetHostByNameRespMsg.h>
#include "helperd/LogMsgEx.h"
#include <common/net/message/helperd/LogRespMsg.h>


#include <common/net/message/SimpleMsg.h>
#include "NetMessageFactory.h"

/**
 * @return NetMessage that must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
std::unique_ptr<NetMessage> NetMessageFactory::createFromMsgType(unsigned short msgType) const
{
   NetMessage* msg;

   switch(msgType)
   {
      // The following lines are grouped by "type of the message" and ordered alphabetically inside
      // the groups. There should always be one message per line to keep a clear layout (although
      // this might lead to lines that are longer than usual)

      // control messages
      case NETMSGTYPE_GenericResponse: { msg = new GenericResponseMsg(); } break;
      case NETMSGTYPE_SetChannelDirect: { msg = new SetChannelDirectMsgEx(); } break;
      case NETMSGTYPE_AuthenticateChannel: { msg = new AuthenticateChannelMsgEx(); } break;
      case NETMSGTYPE_PeerInfo: { msg = new PeerInfoMsgEx(); } break;

      // helperd messages
      case NETMSGTYPE_GetHostByName: { msg = new GetHostByNameMsgEx(); } break;
      case NETMSGTYPE_GetHostByNameResp: { msg = new GetHostByNameRespMsg(); } break;
      case NETMSGTYPE_Log: { msg = new LogMsgEx(); } break;
      case NETMSGTYPE_LogResp: { msg = new LogRespMsg(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return std::unique_ptr<NetMessage>(msg);
}

