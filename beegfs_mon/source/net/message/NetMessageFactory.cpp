#include <common/net/message/SimpleMsg.h>
#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/admon/RequestMetaDataRespMsg.h>
#include <common/net/message/admon/RequestStorageDataRespMsg.h>
#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/net/message/control/DummyMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/GetClientStatsRespMsg.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>

#include <net/message/nodes/HeartbeatMsgEx.h>

#include "NetMessageFactory.h"

/**
 * @return NetMessage that must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
NetMessage* NetMessageFactory::createFromMsgType(unsigned short msgType)
{
   NetMessage* msg;

   switch(msgType)
   {
      // The following lines shoudle be grouped by "type of the message" and ordered alphabetically
      // inside the groups. There should always be one message per line to keep a clear layout
      // (although this might lead to lines that are longer than usual)

      case NETMSGTYPE_FindOwnerResp: { msg = new FindOwnerRespMsg(); } break;
      case NETMSGTYPE_GenericResponse: { msg = new GenericResponseMsg(); } break;
      case NETMSGTYPE_GetClientStatsResp: { msg = new GetClientStatsRespMsg(); } break;
      case NETMSGTYPE_GetMirrorBuddyGroupsResp: { msg = new GetMirrorBuddyGroupsRespMsg(); } break;
      case NETMSGTYPE_GetNodeInfoResp: { msg = new GetNodeInfoRespMsg(); } break;
      case NETMSGTYPE_GetNodesResp: { msg = new GetNodesRespMsg(); } break;
      case NETMSGTYPE_GetTargetMappingsResp: { msg = new GetTargetMappingsRespMsg(); } break;
      case NETMSGTYPE_Heartbeat: { msg = new HeartbeatMsgEx(); } break;
      case NETMSGTYPE_RequestMetaDataResp: { msg = new RequestMetaDataRespMsg(); } break;
      case NETMSGTYPE_RequestStorageDataResp: { msg = new RequestStorageDataRespMsg(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return msg;
}

