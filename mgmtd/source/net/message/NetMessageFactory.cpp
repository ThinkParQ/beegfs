// control messages
#include <common/net/message/control/AuthenticateChannelMsgEx.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/control/PeerInfoMsgEx.h>
#include <net/message/control/AckMsgEx.h>
#include <net/message/control/SetChannelDirectMsgEx.h>

// nodes messages
#include <common/net/message/nodes/ChangeTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/RemoveBuddyGroupRespMsg.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <net/message/nodes/ChangeTargetConsistencyStatesMsgEx.h>
#include <net/message/nodes/GenericDebugMsgEx.h>
#include <net/message/nodes/GetMirrorBuddyGroupsMsgEx.h>
#include <net/message/nodes/GetNodeCapacityPoolsMsgEx.h>
#include <net/message/nodes/GetNodesMsgEx.h>
#include <net/message/nodes/GetStatesAndBuddyGroupsMsgEx.h>
#include <net/message/nodes/GetTargetMappingsMsgEx.h>
#include <net/message/nodes/GetTargetStatesMsgEx.h>
#include <net/message/nodes/HeartbeatMsgEx.h>
#include <net/message/nodes/HeartbeatRequestMsgEx.h>
#include <net/message/nodes/MapTargetsMsgEx.h>
#include <net/message/nodes/RefreshCapacityPoolsMsgEx.h>
#include <net/message/nodes/RegisterNodeMsgEx.h>
#include <net/message/nodes/RegisterTargetMsgEx.h>
#include <net/message/nodes/RemoveBuddyGroupMsgEx.h>
#include <net/message/nodes/RemoveNodeMsgEx.h>
#include <net/message/nodes/SetMirrorBuddyGroupMsgEx.h>
#include <net/message/nodes/SetTargetConsistencyStatesMsgEx.h>
#include <net/message/nodes/UnmapTargetMsgEx.h>
#include <net/message/nodes/storagepools/AddStoragePoolMsgEx.h>
#include <net/message/nodes/storagepools/GetStoragePoolsMsgEx.h>
#include <net/message/nodes/storagepools/ModifyStoragePoolMsgEx.h>
#include <net/message/nodes/storagepools/RemoveStoragePoolMsgEx.h>

// storage messages
#include <common/net/message/storage/mirroring/SetMetadataMirroringRespMsg.h>
#include <common/net/message/storage/quota/GetQuotaInfoRespMsg.h>
#include <common/net/message/storage/quota/SetExceededQuotaRespMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <net/message/storage/mirroring/SetMetadataMirroringMsgEx.h>
#include <net/message/storage/quota/GetDefaultQuotaMsgEx.h>
#include <net/message/storage/quota/SetDefaultQuotaMsgEx.h>
#include <net/message/storage/quota/SetQuotaMsgEx.h>
#include <net/message/storage/quota/RequestExceededQuotaMsgEx.h>
#include <net/message/storage/quota/GetQuotaInfoMsgEx.h>
#include <net/message/storage/GetHighResStatsMsgEx.h>
#include <net/message/storage/SetStorageTargetInfoMsgEx.h>


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
      case NETMSGTYPE_Ack: { msg = new AckMsgEx(); } break;
      case NETMSGTYPE_AuthenticateChannel: { msg = new AuthenticateChannelMsgEx(); } break;
      case NETMSGTYPE_GenericResponse: { msg = new GenericResponseMsg(); } break;
      case NETMSGTYPE_SetChannelDirect: { msg = new SetChannelDirectMsgEx(); } break;
      case NETMSGTYPE_PeerInfo: { msg = new PeerInfoMsgEx(); } break;

      // nodes messages

      case NETMSGTYPE_AddStoragePool: { msg = new AddStoragePoolMsgEx(); } break;
      case NETMSGTYPE_ChangeTargetConsistencyStates: { msg = new ChangeTargetConsistencyStatesMsgEx(); } break;
      case NETMSGTYPE_GenericDebug: { msg = new GenericDebugMsgEx(); } break;
      case NETMSGTYPE_GetMirrorBuddyGroups: { msg = new GetMirrorBuddyGroupsMsgEx(); } break;
      case NETMSGTYPE_GetNodeCapacityPools: { msg = new GetNodeCapacityPoolsMsgEx(); } break;
      case NETMSGTYPE_GetNodeCapacityPoolsResp: { msg = new GetNodeCapacityPoolsRespMsg(); } break;
      case NETMSGTYPE_GetNodes: { msg = new GetNodesMsgEx(); } break;
      case NETMSGTYPE_GetNodesResp: { msg = new GetNodesRespMsg(); } break;
      case NETMSGTYPE_GetStatesAndBuddyGroups: { msg = new GetStatesAndBuddyGroupsMsgEx(); } break;
      case NETMSGTYPE_GetStoragePools: { msg = new GetStoragePoolsMsgEx(); } break;
      case NETMSGTYPE_GetTargetMappings: { msg = new GetTargetMappingsMsgEx(); } break;
      case NETMSGTYPE_GetTargetStates: { msg = new GetTargetStatesMsgEx(); } break;
      case NETMSGTYPE_HeartbeatRequest: { msg = new HeartbeatRequestMsgEx(); } break;
      case NETMSGTYPE_Heartbeat: { msg = new HeartbeatMsgEx(); } break;
      case NETMSGTYPE_MapTargets: { msg = new MapTargetsMsgEx(); } break;
      case NETMSGTYPE_ModifyStoragePool: { msg = new ModifyStoragePoolMsgEx(); } break;
      case NETMSGTYPE_RefreshCapacityPools: { msg = new RefreshCapacityPoolsMsgEx(); } break;
      case NETMSGTYPE_RegisterNode: { msg = new RegisterNodeMsgEx(); } break;
      case NETMSGTYPE_RegisterTarget: { msg = new RegisterTargetMsgEx(); } break;
      case NETMSGTYPE_RemoveBuddyGroup: { msg = new RemoveBuddyGroupMsgEx(); } break;
      case NETMSGTYPE_RemoveBuddyGroupResp: { msg = new RemoveBuddyGroupRespMsg(); } break;
      case NETMSGTYPE_RemoveNode: { msg = new RemoveNodeMsgEx(); } break;
      case NETMSGTYPE_RemoveNodeResp: { msg = new RemoveNodeRespMsg(); } break;
      case NETMSGTYPE_RemoveStoragePool: { msg = new RemoveStoragePoolMsgEx(); } break;
      case NETMSGTYPE_SetMirrorBuddyGroup: { msg = new SetMirrorBuddyGroupMsgEx(); } break;
      case NETMSGTYPE_SetTargetConsistencyStates: { msg = new SetTargetConsistencyStatesMsgEx(); } break;
      case NETMSGTYPE_SetTargetConsistencyStatesResp: { msg = new SetTargetConsistencyStatesRespMsg(); } break;
      case NETMSGTYPE_UnmapTarget: { msg = new UnmapTargetMsgEx(); } break;

      // storage messages
      case NETMSGTYPE_GetHighResStats: { msg = new GetHighResStatsMsgEx(); } break;
      case NETMSGTYPE_StatStoragePathResp: { msg = new StatStoragePathRespMsg(); } break;
      case NETMSGTYPE_SetQuota: { msg = new SetQuotaMsgEx(); } break;
      case NETMSGTYPE_SetExceededQuotaResp: { msg = new SetExceededQuotaRespMsg(); } break;
      case NETMSGTYPE_SetStorageTargetInfo: { msg = new SetStorageTargetInfoMsgEx(); } break;
      case NETMSGTYPE_RequestExceededQuota: { msg = new RequestExceededQuotaMsgEx(); } break;
      case NETMSGTYPE_GetQuotaInfo: { msg = new GetQuotaInfoMsgEx(); } break;
      case NETMSGTYPE_GetQuotaInfoResp: { msg = new GetQuotaInfoRespMsg(); } break;
      case NETMSGTYPE_GetDefaultQuota: { msg = new GetDefaultQuotaMsgEx(); } break;
      case NETMSGTYPE_SetDefaultQuota: { msg = new SetDefaultQuotaMsgEx(); } break;
      case NETMSGTYPE_SetMetadataMirroring: { msg = new SetMetadataMirroringMsgEx(); } break;
      case NETMSGTYPE_SetMetadataMirroringResp: { msg = new SetMetadataMirroringRespMsg(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return std::unique_ptr<NetMessage>(msg);
}

