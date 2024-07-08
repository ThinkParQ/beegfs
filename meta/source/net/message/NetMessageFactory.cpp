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
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/storagepools/GetStoragePoolsRespMsg.h>
#include <net/message/nodes/GenericDebugMsgEx.h>
#include <net/message/nodes/GetClientStatsMsgEx.h>
#include <net/message/nodes/GetNodeCapacityPoolsMsgEx.h>
#include <net/message/nodes/GetNodesMsgEx.h>
#include <net/message/nodes/GetTargetMappingsMsgEx.h>
#include <net/message/nodes/HeartbeatMsgEx.h>
#include <net/message/nodes/HeartbeatRequestMsgEx.h>
#include <net/message/nodes/MapTargetsMsgEx.h>
#include <net/message/nodes/PublishCapacitiesMsgEx.h>
#include <net/message/nodes/RefreshCapacityPoolsMsgEx.h>
#include <net/message/nodes/RemoveNodeMsgEx.h>
#include <net/message/nodes/RefreshTargetStatesMsgEx.h>
#include <net/message/nodes/SetMirrorBuddyGroupMsgEx.h>
#include <net/message/nodes/SetTargetConsistencyStatesMsgEx.h>
#include <net/message/nodes/storagepools/RefreshStoragePoolsMsgEx.h>

// storage messages
#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/net/message/storage/creating/MkFileWithPatternRespMsg.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>
#include <common/net/message/storage/creating/RmChunkPathsRespMsg.h>
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include <common/net/message/storage/mirroring/MirrorMetadataRespMsg.h>
#include <common/net/message/storage/mirroring/ResyncRawInodesRespMsg.h>
#include <common/net/message/storage/mirroring/ResyncSessionStoreRespMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <common/net/message/storage/quota/RequestExceededQuotaRespMsg.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/net/message/storage/TruncFileRespMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileInodeRespMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/RemoveXAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetXAttrRespMsg.h>
#include <common/net/message/storage/creating/HardlinkRespMsg.h>
#include <common/net/message/storage/attribs/UpdateDirParentRespMsg.h>
#include <common/net/message/storage/SetStorageTargetInfoRespMsg.h>
#include <common/net/message/storage/mirroring/StorageResyncStartedRespMsg.h>
#include <net/message/storage/lookup/FindOwnerMsgEx.h>
#include <net/message/storage/listing/ListDirFromOffsetMsgEx.h>
#include <net/message/storage/creating/MkDirMsgEx.h>
#include <net/message/storage/creating/MkFileMsgEx.h>
#include <net/message/storage/creating/MkFileWithPatternMsgEx.h>
#include <net/message/storage/creating/MkLocalDirMsgEx.h>
#include <net/message/storage/creating/RmDirMsgEx.h>
#include <net/message/storage/creating/RmLocalDirMsgEx.h>
#include <net/message/storage/creating/UnlinkLocalFileInodeMsgEx.h>
#include <net/message/storage/creating/RmDirEntryMsgEx.h>
#include <net/message/storage/mirroring/GetMetaResyncStatsMsgEx.h>
#include <net/message/storage/mirroring/ResyncSessionStoreMsgEx.h>
#include <net/message/storage/mirroring/SetMetadataMirroringMsgEx.h>
#include <net/message/storage/mirroring/StorageResyncStartedMsgEx.h>
#include <net/message/storage/moving/MovingDirInsertMsgEx.h>
#include <net/message/storage/moving/MovingFileInsertMsgEx.h>
#include <net/message/storage/moving/RenameV2MsgEx.h>
#include <net/message/storage/quota/SetExceededQuotaMsgEx.h>
#include <net/message/storage/attribs/GetEntryInfoMsgEx.h>
#include <net/message/storage/lookup/LookupIntentMsgEx.h>
#include <net/message/storage/attribs/GetXAttrMsgEx.h>
#include <net/message/storage/attribs/ListXAttrMsgEx.h>
#include <net/message/storage/attribs/RemoveXAttrMsgEx.h>
#include <net/message/storage/attribs/SetAttrMsgEx.h>
#include <net/message/storage/attribs/SetDirPatternMsgEx.h>
#include <net/message/storage/attribs/SetXAttrMsgEx.h>
#include <net/message/storage/attribs/StatMsgEx.h>
#include <net/message/storage/attribs/UpdateDirParentMsgEx.h>
#include <net/message/storage/StatStoragePathMsgEx.h>
#include <net/message/storage/TruncFileMsgEx.h>
#include <net/message/storage/creating/UnlinkFileMsgEx.h>
#include <net/message/storage/GetHighResStatsMsgEx.h>
#include <net/message/storage/attribs/RefreshEntryInfoMsgEx.h>
#include <net/message/storage/lookup/FindLinkOwnerMsgEx.h>
#include <net/message/storage/creating/HardlinkMsgEx.h>
#include <net/message/storage/creating/MoveFileInodeMsgEx.h>
#include <net/message/storage/attribs/UpdateDirParentMsgEx.h>
#include <net/message/storage/mirroring/ResyncRawInodesMsgEx.h>

// session messages
#include <common/net/message/session/AckNotifyRespMsg.h>
#include <common/net/message/session/BumpFileVersionRespMsg.h>
#include <common/net/message/session/FSyncLocalFileRespMsg.h>
#include <common/net/message/session/locking/FLockEntryRespMsg.h>
#include <common/net/message/session/locking/FLockRangeRespMsg.h>
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <net/message/session/AckNotifyMsgEx.h>
#include <net/message/session/BumpFileVersionMsgEx.h>
#include <net/message/session/GetFileVersionMsgEx.h>
#include <net/message/session/locking/FLockAppendMsgEx.h>
#include <net/message/session/locking/FLockEntryMsgEx.h>
#include <net/message/session/locking/FLockRangeMsgEx.h>
#include <net/message/session/opening/CloseFileMsgEx.h>
#include <net/message/session/opening/OpenFileMsgEx.h>

// mon message
#include <net/message/mon/RequestMetaDataMsgEx.h>

// fsck messages
#include <net/message/fsck/CreateDefDirInodesMsgEx.h>
#include <net/message/fsck/CreateEmptyContDirsMsgEx.h>
#include <net/message/fsck/DeleteDirEntriesMsgEx.h>
#include <net/message/fsck/FixInodeOwnersMsgEx.h>
#include <net/message/fsck/FixInodeOwnersInDentryMsgEx.h>
#include <net/message/fsck/LinkToLostAndFoundMsgEx.h>
#include <net/message/fsck/RecreateFsIDsMsgEx.h>
#include <net/message/fsck/RecreateDentriesMsgEx.h>
#include <net/message/fsck/RemoveInodesMsgEx.h>
#include <net/message/fsck/RetrieveDirEntriesMsgEx.h>
#include <net/message/fsck/RetrieveInodesMsgEx.h>
#include <net/message/fsck/RetrieveFsIDsMsgEx.h>
#include <net/message/fsck/FsckSetEventLoggingMsgEx.h>
#include <net/message/fsck/UpdateDirAttribsMsgEx.h>
#include <net/message/fsck/UpdateFileAttribsMsgEx.h>
#include <net/message/fsck/AdjustChunkPermissionsMsgEx.h>
#include <net/message/fsck/CheckAndRepairDupInodeMsgEx.h>

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
      case NETMSGTYPE_GetNodeCapacityPools: { msg = new GetNodeCapacityPoolsMsgEx(); } break;
      case NETMSGTYPE_GetNodeCapacityPoolsResp: { msg = new GetNodeCapacityPoolsRespMsg(); } break;
      case NETMSGTYPE_GetNodes: { msg = new GetNodesMsgEx(); } break;
      case NETMSGTYPE_GetNodesResp: { msg = new GetNodesRespMsg(); } break;
      case NETMSGTYPE_GetStatesAndBuddyGroupsResp: { msg = new GetStatesAndBuddyGroupsRespMsg(); } break;
      case NETMSGTYPE_GetStoragePoolsResp: { msg = new GetStoragePoolsRespMsg(); } break;
      case NETMSGTYPE_GetTargetMappings: { msg = new GetTargetMappingsMsgEx(); } break;
      case NETMSGTYPE_GetTargetMappingsResp: { msg = new GetTargetMappingsRespMsg(); } break;
      case NETMSGTYPE_GetTargetStatesResp: { msg = new GetTargetStatesRespMsg(); } break;
      case NETMSGTYPE_HeartbeatRequest: { msg = new HeartbeatRequestMsgEx(); } break;
      case NETMSGTYPE_Heartbeat: { msg = new HeartbeatMsgEx(); } break;
      case NETMSGTYPE_MapTargets: { msg = new MapTargetsMsgEx(); } break;
      case NETMSGTYPE_PublishCapacities: { msg = new PublishCapacitiesMsgEx(); } break;
      case NETMSGTYPE_RefreshStoragePools: { msg = new RefreshStoragePoolsMsgEx(); } break;
      case NETMSGTYPE_RegisterNodeResp: { msg = new RegisterNodeRespMsg(); } break;
      case NETMSGTYPE_RemoveNode: { msg = new RemoveNodeMsgEx(); } break;
      case NETMSGTYPE_RemoveNodeResp: { msg = new RemoveNodeRespMsg(); } break;
      case NETMSGTYPE_RefreshCapacityPools: { msg = new RefreshCapacityPoolsMsgEx(); } break;
      case NETMSGTYPE_RefreshTargetStates: { msg = new RefreshTargetStatesMsgEx(); } break;
      case NETMSGTYPE_SetMirrorBuddyGroup: { msg = new SetMirrorBuddyGroupMsgEx(); } break;
      case NETMSGTYPE_SetTargetConsistencyStates: { msg = new SetTargetConsistencyStatesMsgEx(); } break;
      case NETMSGTYPE_SetTargetConsistencyStatesResp: { msg = new SetTargetConsistencyStatesRespMsg(); } break;

      // storage messages
      case NETMSGTYPE_FindLinkOwner: { msg = new FindLinkOwnerMsgEx(); } break;
      case NETMSGTYPE_FindOwner: { msg = new FindOwnerMsgEx(); } break;
      case NETMSGTYPE_FindOwnerResp: { msg = new FindOwnerRespMsg(); } break;
      case NETMSGTYPE_GetChunkFileAttribsResp: { msg = new GetChunkFileAttribsRespMsg(); } break;
      case NETMSGTYPE_GetEntryInfo: { msg = new GetEntryInfoMsgEx(); } break;
      case NETMSGTYPE_GetEntryInfoResp: { msg = new GetEntryInfoRespMsg(); } break;
      case NETMSGTYPE_GetHighResStats: { msg = new GetHighResStatsMsgEx(); } break;
      case NETMSGTYPE_GetMetaResyncStats: { msg = new GetMetaResyncStatsMsgEx(); } break;
      case NETMSGTYPE_RequestExceededQuotaResp: {msg = new RequestExceededQuotaRespMsg(); } break;
      case NETMSGTYPE_SetExceededQuota: {msg = new SetExceededQuotaMsgEx(); } break;
      case NETMSGTYPE_StorageResyncStarted: { msg = new StorageResyncStartedMsgEx(); } break;
      case NETMSGTYPE_StorageResyncStartedResp: { msg = new StorageResyncStartedRespMsg(); } break;
      case NETMSGTYPE_GetXAttr: { msg = new GetXAttrMsgEx(); } break;
      case NETMSGTYPE_GetXAttrResp: { msg = new GetXAttrRespMsg(); } break;
      case NETMSGTYPE_Hardlink: { msg = new HardlinkMsgEx(); } break;
      case NETMSGTYPE_HardlinkResp: { msg = new HardlinkRespMsg(); } break;
      case NETMSGTYPE_ListDirFromOffset: { msg = new ListDirFromOffsetMsgEx(); } break;
      case NETMSGTYPE_ListDirFromOffsetResp: { msg = new ListDirFromOffsetRespMsg(); } break;
      case NETMSGTYPE_ListXAttr: { msg = new ListXAttrMsgEx(); } break;
      case NETMSGTYPE_ListXAttrResp: { msg = new ListXAttrRespMsg(); } break;
      case NETMSGTYPE_LookupIntent: { msg = new LookupIntentMsgEx(); } break;
      case NETMSGTYPE_LookupIntentResp: { msg = new LookupIntentRespMsg(); } break;
      case NETMSGTYPE_MkDir: { msg = new MkDirMsgEx(); } break;
      case NETMSGTYPE_MkDirResp: { msg = new MkDirRespMsg(); } break;
      case NETMSGTYPE_MkFile: { msg = new MkFileMsgEx(); } break;
      case NETMSGTYPE_MkFileResp: { msg = new MkFileRespMsg(); } break;
      case NETMSGTYPE_MkFileWithPattern: { msg = new MkFileWithPatternMsgEx(); } break;
      case NETMSGTYPE_MkFileWithPatternResp: { msg = new MkFileWithPatternRespMsg(); } break;
      case NETMSGTYPE_MkLocalDir: { msg = new MkLocalDirMsgEx(); } break;
      case NETMSGTYPE_MkLocalDirResp: { msg = new MkLocalDirRespMsg(); } break;
      case NETMSGTYPE_MovingDirInsert: { msg = new MovingDirInsertMsgEx(); } break;
      case NETMSGTYPE_MovingDirInsertResp: { msg = new MovingDirInsertRespMsg(); } break;
      case NETMSGTYPE_MovingFileInsert: { msg = new MovingFileInsertMsgEx(); } break;
      case NETMSGTYPE_MovingFileInsertResp: { msg = new MovingFileInsertRespMsg(); } break;
      case NETMSGTYPE_RefreshEntryInfo: { msg = new RefreshEntryInfoMsgEx(); } break;
      case NETMSGTYPE_RefreshEntryInfoResp: { msg = new RefreshEntryInfoRespMsg(); } break;
      case NETMSGTYPE_ResyncRawInodes: { msg = new ResyncRawInodesMsgEx(); } break;
      case NETMSGTYPE_ResyncRawInodesResp: { msg = new ResyncRawInodesRespMsg(); } break;
      case NETMSGTYPE_ResyncSessionStore: { msg = new ResyncSessionStoreMsgEx(); } break;
      case NETMSGTYPE_ResyncSessionStoreResp: { msg = new ResyncSessionStoreRespMsg(); } break;
      case NETMSGTYPE_RemoveXAttr: { msg = new RemoveXAttrMsgEx(); } break;
      case NETMSGTYPE_RemoveXAttrResp: { msg = new RemoveXAttrRespMsg(); } break;
      case NETMSGTYPE_Rename: { msg = new RenameV2MsgEx(); } break;
      case NETMSGTYPE_RenameResp: { msg = new RenameRespMsg(); } break;
      case NETMSGTYPE_RmChunkPathsResp: { msg = new RmChunkPathsRespMsg(); } break;
      case NETMSGTYPE_RmDirEntry: { msg = new RmDirEntryMsgEx(); } break;
      case NETMSGTYPE_RmDir: { msg = new RmDirMsgEx(); } break;
      case NETMSGTYPE_RmDirResp: { msg = new RmDirRespMsg(); } break;
      case NETMSGTYPE_RmLocalDir: { msg = new RmLocalDirMsgEx(); } break;
      case NETMSGTYPE_RmLocalDirResp: { msg = new RmLocalDirRespMsg(); } break;
      case NETMSGTYPE_SetAttr: { msg = new SetAttrMsgEx(); } break;
      case NETMSGTYPE_SetAttrResp: { msg = new SetAttrRespMsg(); } break;
      case NETMSGTYPE_SetDirPattern: { msg = new SetDirPatternMsgEx(); } break;
      case NETMSGTYPE_SetDirPatternResp: { msg = new SetDirPatternRespMsg(); } break;
      case NETMSGTYPE_SetLocalAttrResp: { msg = new SetLocalAttrRespMsg(); } break;
      case NETMSGTYPE_SetMetadataMirroring: { msg = new SetMetadataMirroringMsgEx(); } break;
      case NETMSGTYPE_SetStorageTargetInfoResp: { msg = new SetStorageTargetInfoRespMsg(); } break;
      case NETMSGTYPE_SetXAttr: { msg = new SetXAttrMsgEx(); } break;
      case NETMSGTYPE_SetXAttrResp: { msg = new SetXAttrRespMsg(); } break;
      case NETMSGTYPE_Stat: { msg = new StatMsgEx(); } break;
      case NETMSGTYPE_StatResp: { msg = new StatRespMsg(); } break;
      case NETMSGTYPE_StatStoragePath: { msg = new StatStoragePathMsgEx(); } break;
      case NETMSGTYPE_StatStoragePathResp: { msg = new StatStoragePathRespMsg(); } break;
      case NETMSGTYPE_TruncFile: { msg = new TruncFileMsgEx(); } break;
      case NETMSGTYPE_TruncFileResp: { msg = new TruncFileRespMsg(); } break;
      case NETMSGTYPE_TruncLocalFileResp: { msg = new TruncLocalFileRespMsg(); } break;
      case NETMSGTYPE_UnlinkFile: { msg = new UnlinkFileMsgEx(); } break;
      case NETMSGTYPE_UnlinkFileResp: { msg = new UnlinkFileRespMsg(); } break;
      case NETMSGTYPE_UnlinkLocalFileResp: { msg = new UnlinkLocalFileRespMsg(); } break;
      case NETMSGTYPE_UpdateDirParent: { msg = new UpdateDirParentMsgEx(); } break;
      case NETMSGTYPE_UpdateDirParentResp: { msg = new UpdateDirParentRespMsg(); } break;
      case NETMSGTYPE_MoveFileInode: { msg = new MoveFileInodeMsgEx(); } break;
      case NETMSGTYPE_MoveFileInodeResp: {msg = new MoveFileInodeRespMsg(); } break;
      case NETMSGTYPE_UnlinkLocalFileInode: {msg = new UnlinkLocalFileInodeMsgEx(); } break;
      case NETMSGTYPE_UnlinkLocalFileInodeResp: {msg = new UnlinkLocalFileInodeRespMsg(); } break;

      // session messages
      case NETMSGTYPE_BumpFileVersion: { msg = new BumpFileVersionMsgEx(); } break;
      case NETMSGTYPE_BumpFileVersionResp: { msg = new BumpFileVersionRespMsg(); } break;
      case NETMSGTYPE_OpenFile: { msg = new OpenFileMsgEx(); } break;
      case NETMSGTYPE_OpenFileResp: { msg = new OpenFileRespMsg(); } break;
      case NETMSGTYPE_CloseFile: { msg = new CloseFileMsgEx(); } break;
      case NETMSGTYPE_CloseFileResp: { msg = new CloseFileRespMsg(); } break;
      case NETMSGTYPE_CloseChunkFileResp: { msg = new CloseChunkFileRespMsg(); } break;
      case NETMSGTYPE_WriteLocalFileResp: { msg = new WriteLocalFileRespMsg(); } break;
      case NETMSGTYPE_FSyncLocalFileResp: { msg = new FSyncLocalFileRespMsg(); } break;
      case NETMSGTYPE_FLockAppend: { msg = new FLockAppendMsgEx(); } break;
      case NETMSGTYPE_FLockAppendResp: { msg = new FLockAppendRespMsg(); } break;
      case NETMSGTYPE_FLockEntry: { msg = new FLockEntryMsgEx(); } break;
      case NETMSGTYPE_FLockEntryResp: { msg = new FLockEntryRespMsg(); } break;
      case NETMSGTYPE_FLockRange: { msg = new FLockRangeMsgEx(); } break;
      case NETMSGTYPE_FLockRangeResp: { msg = new FLockRangeRespMsg(); } break;
      case NETMSGTYPE_GetFileVersion: { msg = new GetFileVersionMsgEx(); } break;
      case NETMSGTYPE_GetFileVersionResp: { msg = new GetFileVersionRespMsg(); } break;
      case NETMSGTYPE_AckNotify: { msg = new AckNotifiyMsgEx(); } break;
      case NETMSGTYPE_AckNotifyResp: { msg = new AckNotifiyRespMsg(); } break;

      // mon message
      case NETMSGTYPE_RequestMetaData: { msg = new RequestMetaDataMsgEx(); } break;

      // fsck messages
      case NETMSGTYPE_RetrieveDirEntries: { msg = new RetrieveDirEntriesMsgEx(); } break;
      case NETMSGTYPE_RetrieveInodes: { msg = new RetrieveInodesMsgEx(); } break;
      case NETMSGTYPE_RetrieveFsIDs: { msg = new RetrieveFsIDsMsgEx(); } break;
      case NETMSGTYPE_DeleteDirEntries: { msg = new DeleteDirEntriesMsgEx(); } break;
      case NETMSGTYPE_CreateDefDirInodes: { msg = new CreateDefDirInodesMsgEx(); } break;
      case NETMSGTYPE_FixInodeOwners: { msg = new FixInodeOwnersMsgEx(); } break;
      case NETMSGTYPE_FixInodeOwnersInDentry: { msg = new FixInodeOwnersInDentryMsgEx(); } break;
      case NETMSGTYPE_LinkToLostAndFound: { msg = new LinkToLostAndFoundMsgEx(); } break;
      case NETMSGTYPE_CreateEmptyContDirs: { msg = new CreateEmptyContDirsMsgEx(); } break;
      case NETMSGTYPE_UpdateFileAttribs: { msg = new UpdateFileAttribsMsgEx(); } break;
      case NETMSGTYPE_UpdateDirAttribs: { msg = new UpdateDirAttribsMsgEx(); } break;
      case NETMSGTYPE_RemoveInodes: { msg = new RemoveInodesMsgEx(); } break;
      case NETMSGTYPE_RecreateFsIDs: { msg = new RecreateFsIDsMsgEx(); } break;
      case NETMSGTYPE_RecreateDentries: { msg = new RecreateDentriesMsgEx(); } break;
      case NETMSGTYPE_FsckSetEventLogging: { msg = new FsckSetEventLoggingMsgEx(); } break;
      case NETMSGTYPE_AdjustChunkPermissions: { msg = new AdjustChunkPermissionsMsgEx(); } break;
      case NETMSGTYPE_CheckAndRepairDupInode: { msg = new CheckAndRepairDupInodeMsgEx(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return std::unique_ptr<NetMessage>(msg);
}

