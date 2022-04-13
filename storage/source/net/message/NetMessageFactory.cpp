// control messages
#include <common/net/message/control/AuthenticateChannelMsgEx.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/control/PeerInfoMsgEx.h>
#include "control/AckMsgEx.h"
#include "control/SetChannelDirectMsgEx.h"

// nodes messages
#include <common/net/message/nodes/ChangeTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/storagepools/GetStoragePoolsRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include <common/net/message/nodes/RegisterTargetRespMsg.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/GetTargetConsistencyStatesRespMsg.h>
#include <net/message/nodes/GenericDebugMsgEx.h>
#include <net/message/nodes/GetClientStatsMsgEx.h>
#include <net/message/nodes/HeartbeatMsgEx.h>
#include <net/message/nodes/HeartbeatRequestMsgEx.h>
#include <net/message/nodes/MapTargetsMsgEx.h>
#include <net/message/nodes/PublishCapacitiesMsgEx.h>
#include <net/message/nodes/RefreshTargetStatesMsgEx.h>
#include <net/message/nodes/RemoveBuddyGroupMsgEx.h>
#include <net/message/nodes/RemoveNodeMsgEx.h>
#include <net/message/nodes/SetMirrorBuddyGroupMsgEx.h>
#include <net/message/nodes/SetTargetConsistencyStatesMsgEx.h>
#include <net/message/nodes/GetTargetConsistencyStatesMsgEx.h>

// storage messages
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/net/message/storage/creating/RmChunkPathsRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/net/message/storage/listing/ListChunkDirIncrementalRespMsg.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include <common/net/message/storage/mirroring/ResyncLocalFileRespMsg.h>
#include <common/net/message/storage/mirroring/StorageResyncStartedRespMsg.h>
#include <common/net/message/storage/quota/GetQuotaInfoMsg.h>
#include <common/net/message/storage/quota/RequestExceededQuotaRespMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include <common/net/message/storage/SetStorageTargetInfoRespMsg.h>
#include <net/message/storage/attribs/GetChunkFileAttribsMsgEx.h>
#include <net/message/storage/attribs/SetLocalAttrMsgEx.h>
#include <net/message/storage/creating/RmChunkPathsMsgEx.h>
#include <net/message/storage/creating/UnlinkLocalFileMsgEx.h>
#include <net/message/storage/listing/ListChunkDirIncrementalMsgEx.h>
#include <net/message/storage/mirroring/GetStorageResyncStatsMsgEx.h>
#include <net/message/storage/mirroring/ResyncLocalFileMsgEx.h>
#include <net/message/storage/mirroring/SetLastBuddyCommOverrideMsgEx.h>
#include <net/message/storage/mirroring/StorageResyncStartedMsgEx.h>
#include <net/message/storage/quota/GetQuotaInfoMsgEx.h>
#include <net/message/storage/quota/SetExceededQuotaMsgEx.h>
#include <net/message/storage/GetHighResStatsMsgEx.h>
#include <net/message/storage/StatStoragePathMsgEx.h>
#include <net/message/storage/TruncLocalFileMsgEx.h>

// session messages
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include <net/message/session/opening/CloseChunkFileMsgEx.h>
#include <net/message/session/rw/ReadLocalFileV2MsgEx.h>
#include <net/message/session/rw/WriteLocalFileMsgEx.h>
#include <net/message/session/FSyncLocalFileMsgEx.h>

#ifdef BEEGFS_NVFS
#include <net/message/session/rw/ReadLocalFileRDMAMsgEx.h>
#include <net/message/session/rw/WriteLocalFileRDMAMsgEx.h>
#endif /* BEEGFS_NVFS */

// mon messages
#include <net/message/mon/RequestStorageDataMsgEx.h>

// fsck
#include <net/message/fsck/DeleteChunksMsgEx.h>
#include <net/message/fsck/FetchFsckChunkListMsgEx.h>
#include <net/message/fsck/MoveChunkFileMsgEx.h>

// storage benchmark
#include <common/net/message/nodes/StorageBenchControlMsg.h>
#include <net/message/nodes/StorageBenchControlMsgEx.h>

#include <common/net/message/SimpleMsg.h>
#include <net/message/nodes/storagepools/RefreshStoragePoolsMsgEx.h>
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
      case NETMSGTYPE_ChangeTargetConsistencyStatesResp: { msg = new ChangeTargetConsistencyStatesRespMsg(); } break;
      case NETMSGTYPE_GenericDebug: { msg = new GenericDebugMsgEx(); } break;
      case NETMSGTYPE_GetClientStats: { msg = new GetClientStatsMsgEx(); } break;
      case NETMSGTYPE_GetMirrorBuddyGroupsResp: { msg = new GetMirrorBuddyGroupsRespMsg(); } break;
      case NETMSGTYPE_GetNodesResp: { msg = new GetNodesRespMsg(); } break;
      case NETMSGTYPE_GetStatesAndBuddyGroupsResp: { msg = new GetStatesAndBuddyGroupsRespMsg(); } break;
      case NETMSGTYPE_GetStoragePoolsResp: { msg = new GetStoragePoolsRespMsg(); } break;
      case NETMSGTYPE_GetTargetMappingsResp: { msg = new GetTargetMappingsRespMsg(); } break;
      case NETMSGTYPE_GetTargetStatesResp: { msg = new GetTargetStatesRespMsg(); } break;
      case NETMSGTYPE_HeartbeatRequest: { msg = new HeartbeatRequestMsgEx(); } break;
      case NETMSGTYPE_Heartbeat: { msg = new HeartbeatMsgEx(); } break;
      case NETMSGTYPE_MapTargets: { msg = new MapTargetsMsgEx(); } break;
      case NETMSGTYPE_PublishCapacities: { msg = new PublishCapacitiesMsgEx(); } break;
      case NETMSGTYPE_MapTargetsResp: { msg = new MapTargetsRespMsg(); } break;
      case NETMSGTYPE_StorageBenchControlMsg: {msg = new StorageBenchControlMsgEx(); } break;
      case NETMSGTYPE_RefreshStoragePools: { msg = new RefreshStoragePoolsMsgEx(); } break;
      case NETMSGTYPE_RefreshTargetStates: { msg = new RefreshTargetStatesMsgEx(); } break;
      case NETMSGTYPE_RegisterNodeResp: { msg = new RegisterNodeRespMsg(); } break;
      case NETMSGTYPE_RegisterTargetResp: { msg = new RegisterTargetRespMsg(); } break;
      case NETMSGTYPE_RemoveBuddyGroup: { msg = new RemoveBuddyGroupMsgEx(); } break;
      case NETMSGTYPE_RemoveNode: { msg = new RemoveNodeMsgEx(); } break;
      case NETMSGTYPE_RemoveNodeResp: { msg = new RemoveNodeRespMsg(); } break;
      case NETMSGTYPE_SetMirrorBuddyGroup: { msg = new SetMirrorBuddyGroupMsgEx(); } break;
      case NETMSGTYPE_SetTargetConsistencyStates: { msg = new SetTargetConsistencyStatesMsgEx(); } break;
      case NETMSGTYPE_SetTargetConsistencyStatesResp: { msg = new SetTargetConsistencyStatesRespMsg(); } break;
      case NETMSGTYPE_GetTargetConsistencyStates: { msg = new GetTargetConsistencyStatesMsgEx(); } break;
      case NETMSGTYPE_GetTargetConsistencyStatesResp: { msg = new GetTargetConsistencyStatesRespMsg(); } break;

      // storage messages
      case NETMSGTYPE_FindOwnerResp: { msg = new FindOwnerRespMsg(); } break;
      case NETMSGTYPE_GetChunkFileAttribs: { msg = new GetChunkFileAttribsMsgEx(); } break;
      case NETMSGTYPE_GetHighResStats: { msg = new GetHighResStatsMsgEx(); } break;
      case NETMSGTYPE_GetQuotaInfo: {msg = new GetQuotaInfoMsgEx(); } break;
      case NETMSGTYPE_GetStorageResyncStats: { msg = new GetStorageResyncStatsMsgEx(); } break;
      case NETMSGTYPE_ListChunkDirIncremental: { msg = new ListChunkDirIncrementalMsgEx(); } break;
      case NETMSGTYPE_ListChunkDirIncrementalResp: { msg = new ListChunkDirIncrementalRespMsg(); } break;
      case NETMSGTYPE_RequestExceededQuotaResp: {msg = new RequestExceededQuotaRespMsg(); } break;
      case NETMSGTYPE_ResyncLocalFile: { msg = new ResyncLocalFileMsgEx(); } break;
      case NETMSGTYPE_ResyncLocalFileResp: { msg = new ResyncLocalFileRespMsg(); } break;
      case NETMSGTYPE_RmChunkPaths: { msg = new RmChunkPathsMsgEx(); } break;
      case NETMSGTYPE_RmChunkPathsResp: { msg = new RmChunkPathsRespMsg(); } break;
      case NETMSGTYPE_SetExceededQuota: {msg = new SetExceededQuotaMsgEx(); } break;
      case NETMSGTYPE_SetLastBuddyCommOverride: { msg = new SetLastBuddyCommOverrideMsgEx(); } break;
      case NETMSGTYPE_SetLocalAttr: { msg = new SetLocalAttrMsgEx(); } break;
      case NETMSGTYPE_SetLocalAttrResp: { msg = new SetLocalAttrRespMsg(); } break;
      case NETMSGTYPE_SetStorageTargetInfoResp: { msg = new SetStorageTargetInfoRespMsg(); } break;
      case NETMSGTYPE_StatStoragePath: { msg = new StatStoragePathMsgEx(); } break;
      case NETMSGTYPE_StorageResyncStarted: { msg = new StorageResyncStartedMsgEx(); } break;
      case NETMSGTYPE_StorageResyncStartedResp: { msg = new StorageResyncStartedRespMsg(); } break;
      case NETMSGTYPE_TruncLocalFile: { msg = new TruncLocalFileMsgEx(); } break;
      case NETMSGTYPE_TruncLocalFileResp: { msg = new TruncLocalFileRespMsg(); } break;
      case NETMSGTYPE_UnlinkLocalFile: { msg = new UnlinkLocalFileMsgEx(); } break;
      case NETMSGTYPE_UnlinkLocalFileResp: { msg = new UnlinkLocalFileRespMsg(); } break;

      // session messages
      case NETMSGTYPE_CloseChunkFile: { msg = new CloseChunkFileMsgEx(); } break;
      case NETMSGTYPE_CloseChunkFileResp: { msg = new CloseChunkFileRespMsg(); } break;
      case NETMSGTYPE_FSyncLocalFile: { msg = new FSyncLocalFileMsgEx(); } break;
      case NETMSGTYPE_ReadLocalFileV2: { msg = new ReadLocalFileV2MsgEx(); } break;
      case NETMSGTYPE_WriteLocalFile: { msg = new WriteLocalFileMsgEx(); } break;
      case NETMSGTYPE_WriteLocalFileResp: { msg = new WriteLocalFileRespMsg(); } break;
#ifdef BEEGFS_NVFS
      case NETMSGTYPE_ReadLocalFileRDMA: { msg = new ReadLocalFileRDMAMsgEx(); } break;
      case NETMSGTYPE_WriteLocalFileRDMA: { msg = new WriteLocalFileRDMAMsgEx(); } break;
#endif // BEEGFS_NVFS

      // mon message
      case NETMSGTYPE_RequestStorageData: { msg = new RequestStorageDataMsgEx(); } break;

      // fsck
      case NETMSGTYPE_DeleteChunks: { msg = new DeleteChunksMsgEx(); } break;
      case NETMSGTYPE_FetchFsckChunkList: { msg = new FetchFsckChunkListMsgEx(); } break;
      case NETMSGTYPE_MoveChunkFile: { msg = new MoveChunkFileMsgEx(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return std::unique_ptr<NetMessage>(msg);
}

