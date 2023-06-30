// control messages
#include <common/net/message/control/DummyMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <net/message/control/AckMsgEx.h>

// nodes messages
#include <common/net/message/nodes/GenericDebugRespMsg.h>
#include <common/net/message/nodes/GetClientStatsRespMsg.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/net/message/nodes/RemoveBuddyGroupRespMsg.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/nodes/SetMirrorBuddyGroupRespMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/UnmapTargetRespMsg.h>
#include <common/net/message/nodes/storagepools/AddStoragePoolRespMsg.h>
#include <common/net/message/nodes/storagepools/GetStoragePoolsRespMsg.h>
#include <common/net/message/nodes/storagepools/ModifyStoragePoolRespMsg.h>
#include <common/net/message/nodes/storagepools/RemoveStoragePoolRespMsg.h>
#include <net/message/nodes/HeartbeatMsgEx.h>
#include <net/message/nodes/HeartbeatRequestMsgEx.h>

// storage messages
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/net/message/storage/creating/MkFileWithPatternRespMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include <common/net/message/storage/creating/RmDirEntryRespMsg.h>
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include <common/net/message/storage/lookup/FindLinkOwnerRespMsg.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/net/message/storage/mirroring/GetMetaResyncStatsRespMsg.h>
#include <common/net/message/storage/mirroring/GetStorageResyncStatsRespMsg.h>
#include <common/net/message/storage/mirroring/SetLastBuddyCommOverrideRespMsg.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringRespMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <common/net/message/storage/quota/GetDefaultQuotaRespMsg.h>
#include <common/net/message/storage/quota/GetQuotaInfoRespMsg.h>
#include <common/net/message/storage/quota/SetDefaultQuotaRespMsg.h>
#include <common/net/message/storage/quota/SetQuotaRespMsg.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/net/message/storage/TruncFileRespMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>

// session messages
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include <common/net/message/session/FSyncLocalFileRespMsg.h>

// storage benchmark
#include <common/net/message/nodes/StorageBenchControlMsgResp.h>

// general includes
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
      case NETMSGTYPE_Dummy: { msg = new DummyMsg(); } break;
      case NETMSGTYPE_GenericResponse: { msg = new GenericResponseMsg(); } break;

      // nodes messages
      case NETMSGTYPE_AddStoragePoolResp: { msg = new AddStoragePoolRespMsg(); } break;
      case NETMSGTYPE_GenericDebugResp: { msg = new GenericDebugRespMsg(); } break;
      case NETMSGTYPE_GetClientStatsResp:       { msg = new GetClientStatsRespMsg(); } break;
      case NETMSGTYPE_GetMirrorBuddyGroupsResp: { msg = new GetMirrorBuddyGroupsRespMsg(); } break;
      case NETMSGTYPE_GetNodeCapacityPoolsResp: { msg = new GetNodeCapacityPoolsRespMsg(); } break;
      case NETMSGTYPE_GetNodesResp: { msg = new GetNodesRespMsg(); } break;
      case NETMSGTYPE_GetStatesAndBuddyGroupsResp: { msg = new GetStatesAndBuddyGroupsRespMsg(); } break;
      case NETMSGTYPE_GetStoragePoolsResp: { msg = new GetStoragePoolsRespMsg(); } break;
      case NETMSGTYPE_GetTargetMappingsResp: { msg = new GetTargetMappingsRespMsg(); } break;
      case NETMSGTYPE_GetTargetStatesResp: { msg = new GetTargetStatesRespMsg(); } break;
      case NETMSGTYPE_HeartbeatRequest: { msg = new HeartbeatRequestMsgEx(); } break;
      case NETMSGTYPE_Heartbeat: { msg = new HeartbeatMsgEx(); } break;
      case NETMSGTYPE_MapTargetsResp: { msg = new MapTargetsRespMsg(); } break;
      case NETMSGTYPE_ModifyStoragePoolResp: { msg = new ModifyStoragePoolRespMsg(); } break;
      case NETMSGTYPE_RemoveBuddyGroupResp: { msg = new RemoveBuddyGroupRespMsg(); } break;
      case NETMSGTYPE_RemoveNodeResp: { msg = new RemoveNodeRespMsg(); } break;
      case NETMSGTYPE_RemoveStoragePoolResp: { msg = new RemoveStoragePoolRespMsg(); } break;
      case NETMSGTYPE_UnmapTargetResp: { msg = new UnmapTargetRespMsg(); } break;
      case NETMSGTYPE_SetMirrorBuddyGroupResp: { msg = new SetMirrorBuddyGroupRespMsg(); } break;
      case NETMSGTYPE_SetTargetConsistencyStatesResp: { msg = new SetTargetConsistencyStatesRespMsg(); } break;
      case NETMSGTYPE_StorageBenchControlMsgResp: { msg = new StorageBenchControlMsgResp(); } break;

      // storage messages
      case NETMSGTYPE_FindLinkOwnerResp: { msg = new FindLinkOwnerRespMsg(); } break;
      case NETMSGTYPE_FindOwnerResp: { msg = new FindOwnerRespMsg(); } break;
      case NETMSGTYPE_GetChunkFileAttribsResp: { msg = new GetChunkFileAttribsRespMsg(); } break;
      case NETMSGTYPE_GetEntryInfoResp: { msg = new GetEntryInfoRespMsg(); } break;
      case NETMSGTYPE_GetHighResStatsResp: { msg = new GetHighResStatsRespMsg(); } break;
      case NETMSGTYPE_GetQuotaInfoResp: { msg = new GetQuotaInfoRespMsg(); } break;
      case NETMSGTYPE_GetMetaResyncStatsResp: { msg = new GetMetaResyncStatsRespMsg(); } break;
      case NETMSGTYPE_GetStorageResyncStatsResp: { msg = new GetStorageResyncStatsRespMsg(); } break;
      case NETMSGTYPE_ListDirFromOffsetResp: { msg = new ListDirFromOffsetRespMsg(); } break;
      case NETMSGTYPE_MkDirResp: { msg = new MkDirRespMsg(); } break;
      case NETMSGTYPE_MkFileWithPatternResp: { msg = new MkFileWithPatternRespMsg(); } break;
      case NETMSGTYPE_MkFileResp: { msg = new MkFileRespMsg(); } break;
      case NETMSGTYPE_MkLocalDirResp: { msg = new MkLocalDirRespMsg(); } break;
      case NETMSGTYPE_MovingDirInsertResp: { msg = new MovingDirInsertRespMsg(); } break;
      case NETMSGTYPE_MovingFileInsertResp: { msg = new MovingFileInsertRespMsg(); } break;
      case NETMSGTYPE_RefreshEntryInfoResp: { msg = new RefreshEntryInfoRespMsg(); } break;
      case NETMSGTYPE_RenameResp: { msg = new RenameRespMsg(); } break;
      case NETMSGTYPE_RmDirResp: { msg = new RmDirRespMsg(); } break;
      case NETMSGTYPE_RmDirEntryResp: { msg = new RmDirEntryRespMsg(); } break;
      case NETMSGTYPE_LookupIntentResp: { msg = new LookupIntentRespMsg(); } break;
      case NETMSGTYPE_RmLocalDirResp: { msg = new RmLocalDirRespMsg(); } break;
      case NETMSGTYPE_SetAttrResp: { msg = new SetAttrRespMsg(); } break;
      case NETMSGTYPE_SetDirPatternResp: { msg = new SetDirPatternRespMsg(); } break;
      case NETMSGTYPE_SetLastBuddyCommOverrideResp: { msg = new SetLastBuddyCommOverrideRespMsg(); } break;
      case NETMSGTYPE_SetLocalAttrResp: { msg = new SetLocalAttrRespMsg(); } break;
      case NETMSGTYPE_SetMetadataMirroringResp: { msg = new SetMetadataMirroringRespMsg(); } break;
      case NETMSGTYPE_SetQuotaResp: { msg = new SetQuotaRespMsg(); } break;
      case NETMSGTYPE_StatResp: { msg = new StatRespMsg(); } break;
      case NETMSGTYPE_StatStoragePathResp: { msg = new StatStoragePathRespMsg(); } break;
      case NETMSGTYPE_TruncFileResp: { msg = new TruncFileRespMsg(); } break;
      case NETMSGTYPE_TruncLocalFileResp: { msg = new TruncLocalFileRespMsg(); } break;
      case NETMSGTYPE_UnlinkFileResp: { msg = new UnlinkFileRespMsg(); } break;
      case NETMSGTYPE_UnlinkLocalFileResp: { msg = new UnlinkLocalFileRespMsg(); } break;
      case NETMSGTYPE_GetDefaultQuotaResp: { msg = new GetDefaultQuotaRespMsg(); } break;
      case NETMSGTYPE_SetDefaultQuotaResp: { msg = new SetDefaultQuotaRespMsg(); } break;
      case NETMSGTYPE_MoveFileInode: { msg = new MoveFileInodeMsg(); } break;
      case NETMSGTYPE_MoveFileInodeResp: {msg = new MoveFileInodeRespMsg(); } break;

      // session messages
      case NETMSGTYPE_CloseChunkFileResp: { msg = new CloseChunkFileRespMsg(); } break;
      case NETMSGTYPE_CloseFileResp: { msg = new CloseFileRespMsg(); } break;
      case NETMSGTYPE_FSyncLocalFileResp: { msg = new FSyncLocalFileRespMsg(); } break;
      case NETMSGTYPE_OpenFileResp: { msg = new OpenFileRespMsg(); } break;
      case NETMSGTYPE_WriteLocalFileResp: { msg = new WriteLocalFileRespMsg(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return std::unique_ptr<NetMessage>(msg);
}

