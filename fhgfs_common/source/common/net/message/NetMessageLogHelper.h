/*
 * Main purpose is to convert NetMessage defines into a string
 */

#ifndef NETMESSAGELOGHELPER_H_
#define NETMESSAGELOGHELPER_H_

#include <common/net/message/NetMessageTypes.h>
#include <common/logging/DefineToStrMapping.h>

/**
 * NetMessageType to human-readable string mapping
 */
class NetMsgStrMapping : public DefineToStrMapping
{
   public:
      NetMsgStrMapping()
      {
         /*
            Assignments generated with shell command:

            sed -ne "s/^\#define NETMSGTYPE_\([^ ]\+\) \+[0-9]\+\( \/\/.\+\)\?$/this->defineToStrMap[NETMSGTYPE_\1] = \"\1\";/p" common/net/message/NetMessageTypes.h
         */

         this->defineToStrMap[NETMSGTYPE_Invalid] = "Invalid";
         this->defineToStrMap[NETMSGTYPE_RemoveNode] = "RemoveNode";
         this->defineToStrMap[NETMSGTYPE_RemoveNodeResp] = "RemoveNodeResp";
         this->defineToStrMap[NETMSGTYPE_GetNodes] = "GetNodes";
         this->defineToStrMap[NETMSGTYPE_GetNodesResp] = "GetNodesResp";
         this->defineToStrMap[NETMSGTYPE_HeartbeatRequest] = "HeartbeatRequest";
         this->defineToStrMap[NETMSGTYPE_Heartbeat] = "Heartbeat";
         this->defineToStrMap[NETMSGTYPE_GetNodeCapacityPools] = "GetNodeCapacityPools";
         this->defineToStrMap[NETMSGTYPE_GetNodeCapacityPoolsResp] = "GetNodeCapacityPoolsResp";
         this->defineToStrMap[NETMSGTYPE_MapTargets] = "MapTargets";
         this->defineToStrMap[NETMSGTYPE_MapTargetsResp] = "MapTargetsResp";
         this->defineToStrMap[NETMSGTYPE_GetTargetMappings] = "GetTargetMappings";
         this->defineToStrMap[NETMSGTYPE_GetTargetMappingsResp] = "GetTargetMappingsResp";
         this->defineToStrMap[NETMSGTYPE_UnmapTarget] = "UnmapTarget";
         this->defineToStrMap[NETMSGTYPE_UnmapTargetResp] = "UnmapTargetResp";
         this->defineToStrMap[NETMSGTYPE_GenericDebug] = "GenericDebug";
         this->defineToStrMap[NETMSGTYPE_GenericDebugResp] = "GenericDebugResp";
         this->defineToStrMap[NETMSGTYPE_GetClientStats] = "GetClientStats";
         this->defineToStrMap[NETMSGTYPE_GetClientStatsResp] = "GetClientStatsResp";
         this->defineToStrMap[NETMSGTYPE_RefresherControl] = "RefresherControl";
         this->defineToStrMap[NETMSGTYPE_RefresherControlResp] = "RefresherControlResp";
         this->defineToStrMap[NETMSGTYPE_RefreshCapacityPools] = "RefreshCapacityPools";
         this->defineToStrMap[NETMSGTYPE_RefreshCapacityPoolsResp] = "RefreshCapacityPoolsResp";
         this->defineToStrMap[NETMSGTYPE_StorageBenchControlMsg] = "StorageBenchControlMsg";
         this->defineToStrMap[NETMSGTYPE_StorageBenchControlMsgResp] = "StorageBenchControlMsgResp";
         this->defineToStrMap[NETMSGTYPE_RegisterNode] = "RegisterNode";
         this->defineToStrMap[NETMSGTYPE_RegisterNodeResp] = "RegisterNodeResp";
         this->defineToStrMap[NETMSGTYPE_RegisterTarget] = "RegisterTarget";
         this->defineToStrMap[NETMSGTYPE_RegisterTargetResp] = "RegisterTargetResp";
         this->defineToStrMap[NETMSGTYPE_SetMirrorBuddyGroup] = "SetMirrorBuddyGroup";
         this->defineToStrMap[NETMSGTYPE_SetMirrorBuddyGroupResp] = "SetMirrorBuddyGroupResp";
         this->defineToStrMap[NETMSGTYPE_GetMirrorBuddyGroups] = "GetMirrorBuddyGroups";
         this->defineToStrMap[NETMSGTYPE_GetMirrorBuddyGroupsResp] = "GetMirrorBuddyGroupsResp";
         this->defineToStrMap[NETMSGTYPE_GetTargetStates] = "GetTargetStates";
         this->defineToStrMap[NETMSGTYPE_GetTargetStatesResp] = "GetTargetStatesResp";
         this->defineToStrMap[NETMSGTYPE_RefreshTargetStates] = "RefreshTargetStates";
         this->defineToStrMap[NETMSGTYPE_RefreshTargetStatesResp] = "RefreshTargetStatesResp";
         this->defineToStrMap[NETMSGTYPE_GetStatesAndBuddyGroups] = "GetStatesAndBuddyGroups";
         this->defineToStrMap[NETMSGTYPE_GetStatesAndBuddyGroupsResp] = "GetStatesAndBuddyGroupsResp";
         this->defineToStrMap[NETMSGTYPE_SetTargetConsistencyStates] = "SetTargetConsistencyStates";
         this->defineToStrMap[NETMSGTYPE_SetTargetConsistencyStatesResp] = "SetTargetConsistencyStatesResp";
         this->defineToStrMap[NETMSGTYPE_ChangeTargetConsistencyStates] = "ChangeTargetConsistencyStates";
         this->defineToStrMap[NETMSGTYPE_ChangeTargetConsistencyStatesResp] = "ChangeTargetConsistencyStatesResp";
         this->defineToStrMap[NETMSGTYPE_PublishCapacities] = "PublishCapacities";
         this->defineToStrMap[NETMSGTYPE_RemoveBuddyGroup] = "RemoveBuddyGroup";
         this->defineToStrMap[NETMSGTYPE_RemoveBuddyGroupResp] = "RemoveBuddyGroupResp";
         this->defineToStrMap[NETMSGTYPE_MkDir] = "MkDir";
         this->defineToStrMap[NETMSGTYPE_MkDirResp] = "MkDirResp";
         this->defineToStrMap[NETMSGTYPE_RmDir] = "RmDir";
         this->defineToStrMap[NETMSGTYPE_RmDirResp] = "RmDirResp";
         this->defineToStrMap[NETMSGTYPE_MkFile] = "MkFile";
         this->defineToStrMap[NETMSGTYPE_MkFileResp] = "MkFileResp";
         this->defineToStrMap[NETMSGTYPE_UnlinkFile] = "UnlinkFile";
         this->defineToStrMap[NETMSGTYPE_UnlinkFileResp] = "UnlinkFileResp";
         this->defineToStrMap[NETMSGTYPE_MkLocalFile] = "MkLocalFile";
         this->defineToStrMap[NETMSGTYPE_MkLocalFileResp] = "MkLocalFileResp";
         this->defineToStrMap[NETMSGTYPE_UnlinkLocalFile] = "UnlinkLocalFile";
         this->defineToStrMap[NETMSGTYPE_UnlinkLocalFileResp] = "UnlinkLocalFileResp";
         this->defineToStrMap[NETMSGTYPE_Stat] = "Stat";
         this->defineToStrMap[NETMSGTYPE_StatResp] = "StatResp";
         this->defineToStrMap[NETMSGTYPE_GetChunkFileAttribs] = "GetChunkFileAttribs";
         this->defineToStrMap[NETMSGTYPE_GetChunkFileAttribsResp] = "GetChunkFileAttribsResp";
         this->defineToStrMap[NETMSGTYPE_TruncFile] = "TruncFile";
         this->defineToStrMap[NETMSGTYPE_TruncFileResp] = "TruncFileResp";
         this->defineToStrMap[NETMSGTYPE_TruncLocalFile] = "TruncLocalFile";
         this->defineToStrMap[NETMSGTYPE_TruncLocalFileResp] = "TruncLocalFileResp";
         this->defineToStrMap[NETMSGTYPE_Rename] = "Rename";
         this->defineToStrMap[NETMSGTYPE_RenameResp] = "RenameResp";
         this->defineToStrMap[NETMSGTYPE_SetAttr] = "SetAttr";
         this->defineToStrMap[NETMSGTYPE_SetAttrResp] = "SetAttrResp";
         this->defineToStrMap[NETMSGTYPE_ListDirFromOffset] = "ListDirFromOffset";
         this->defineToStrMap[NETMSGTYPE_ListDirFromOffsetResp] = "ListDirFromOffsetResp";
         this->defineToStrMap[NETMSGTYPE_StatStoragePath] = "StatStoragePath";
         this->defineToStrMap[NETMSGTYPE_StatStoragePathResp] = "StatStoragePathResp";
         this->defineToStrMap[NETMSGTYPE_SetLocalAttr] = "SetLocalAttr";
         this->defineToStrMap[NETMSGTYPE_SetLocalAttrResp] = "SetLocalAttrResp";
         this->defineToStrMap[NETMSGTYPE_FindOwner] = "FindOwner";
         this->defineToStrMap[NETMSGTYPE_FindOwnerResp] = "FindOwnerResp";
         this->defineToStrMap[NETMSGTYPE_MkLocalDir] = "MkLocalDir";
         this->defineToStrMap[NETMSGTYPE_MkLocalDirResp] = "MkLocalDirResp";
         this->defineToStrMap[NETMSGTYPE_RmLocalDir] = "RmLocalDir";
         this->defineToStrMap[NETMSGTYPE_RmLocalDirResp] = "RmLocalDirResp";
         this->defineToStrMap[NETMSGTYPE_MovingFileInsert] = "MovingFileInsert";
         this->defineToStrMap[NETMSGTYPE_MovingFileInsertResp] = "MovingFileInsertResp";
         this->defineToStrMap[NETMSGTYPE_MovingDirInsert] = "MovingDirInsert";
         this->defineToStrMap[NETMSGTYPE_MovingDirInsertResp] = "MovingDirInsertResp";
         this->defineToStrMap[NETMSGTYPE_GetEntryInfo] = "GetEntryInfo";
         this->defineToStrMap[NETMSGTYPE_GetEntryInfoResp] = "GetEntryInfoResp";
         this->defineToStrMap[NETMSGTYPE_SetDirPattern] = "SetDirPattern";
         this->defineToStrMap[NETMSGTYPE_SetDirPatternResp] = "SetDirPatternResp";
         this->defineToStrMap[NETMSGTYPE_GetHighResStats] = "GetHighResStats";
         this->defineToStrMap[NETMSGTYPE_GetHighResStatsResp] = "GetHighResStatsResp";
         this->defineToStrMap[NETMSGTYPE_MkFileWithPattern] = "MkFileWithPattern";
         this->defineToStrMap[NETMSGTYPE_MkFileWithPatternResp] = "MkFileWithPatternResp";
         this->defineToStrMap[NETMSGTYPE_RefreshEntryInfo] = "RefreshEntryInfo";
         this->defineToStrMap[NETMSGTYPE_RefreshEntryInfoResp] = "RefreshEntryInfoResp";
         this->defineToStrMap[NETMSGTYPE_RmDirEntry] = "RmDirEntry";
         this->defineToStrMap[NETMSGTYPE_RmDirEntryResp] = "RmDirEntryResp";
         this->defineToStrMap[NETMSGTYPE_LookupIntent] = "LookupIntent";
         this->defineToStrMap[NETMSGTYPE_LookupIntentResp] = "LookupIntentResp";
         this->defineToStrMap[NETMSGTYPE_FindEntryname] = "FindEntryname";
         this->defineToStrMap[NETMSGTYPE_FindEntrynameResp] = "FindEntrynameResp";
         this->defineToStrMap[NETMSGTYPE_FindLinkOwner] = "FindLinkOwner";
         this->defineToStrMap[NETMSGTYPE_FindLinkOwnerResp] = "FindLinkOwnerResp";
         this->defineToStrMap[NETMSGTYPE_UpdateBacklink] = "UpdateBacklink";
         this->defineToStrMap[NETMSGTYPE_UpdateBacklinkResp] = "UpdateBacklinkResp";
         this->defineToStrMap[NETMSGTYPE_MirrorMetadata] = "MirrorMetadata";
         this->defineToStrMap[NETMSGTYPE_MirrorMetadataResp] = "MirrorMetadataResp";
         this->defineToStrMap[NETMSGTYPE_SetMetadataMirroring] = "SetMetadataMirroring";
         this->defineToStrMap[NETMSGTYPE_SetMetadataMirroringResp] = "SetMetadataMirroringResp";
         this->defineToStrMap[NETMSGTYPE_Hardlink] = "Hardlink";
         this->defineToStrMap[NETMSGTYPE_HardlinkResp] = "HardlinkResp";
         this->defineToStrMap[NETMSGTYPE_GetStorageTargetInfo] = "GetStorageTargetInfo";
         this->defineToStrMap[NETMSGTYPE_GetStorageTargetInfoResp] = "GetStorageTargetInfoResp";
         this->defineToStrMap[NETMSGTYPE_SetQuota] = "SetQuota";
         this->defineToStrMap[NETMSGTYPE_SetQuotaResp] = "SetQuotaResp";
         this->defineToStrMap[NETMSGTYPE_SetExceededQuota] = "SetExceededQuota";
         this->defineToStrMap[NETMSGTYPE_SetExceededQuotaResp] = "SetExceededQuotaResp";
         this->defineToStrMap[NETMSGTYPE_RequestExceededQuota] = "RequestExceededQuota";
         this->defineToStrMap[NETMSGTYPE_RequestExceededQuotaResp] = "RequestExceededQuotaResp";
         this->defineToStrMap[NETMSGTYPE_UpdateDirParent] = "UpdateDirParent";
         this->defineToStrMap[NETMSGTYPE_UpdateDirParentResp] = "UpdateDirParentResp";
         this->defineToStrMap[NETMSGTYPE_ResyncLocalFile] = "ResyncLocalFile";
         this->defineToStrMap[NETMSGTYPE_ResyncLocalFileResp] = "ResyncLocalFileResp";
         this->defineToStrMap[NETMSGTYPE_StartStorageTargetResync] = "StartStorageTargetResync";
         this->defineToStrMap[NETMSGTYPE_StartStorageTargetResyncResp] = "StartStorageTargetResyncResp";
         this->defineToStrMap[NETMSGTYPE_StorageResyncStarted] = "StorageResyncStarted";
         this->defineToStrMap[NETMSGTYPE_StorageResyncStartedResp] = "StorageResyncStartedResp";
         this->defineToStrMap[NETMSGTYPE_ListChunkDirIncremental] = "ListChunkDirIncremental";
         this->defineToStrMap[NETMSGTYPE_ListChunkDirIncrementalResp] = "ListChunkDirIncrementalResp";
         this->defineToStrMap[NETMSGTYPE_RmChunkPaths] = "RmChunkPaths";
         this->defineToStrMap[NETMSGTYPE_RmChunkPathsResp] = "RmChunkPathsResp";
         this->defineToStrMap[NETMSGTYPE_GetStorageResyncStats] = "GetStorageResyncStats";
         this->defineToStrMap[NETMSGTYPE_GetStorageResyncStatsResp] = "GetStorageResyncStatsResp";
         this->defineToStrMap[NETMSGTYPE_SetLastBuddyCommOverride] = "SetLastBuddyCommOverride";
         this->defineToStrMap[NETMSGTYPE_SetLastBuddyCommOverrideResp] = "SetLastBuddyCommOverrideResp";
         this->defineToStrMap[NETMSGTYPE_GetQuotaInfo] = "GetQuotaInfo";
         this->defineToStrMap[NETMSGTYPE_GetQuotaInfoResp] = "GetQuotaInfoResp";
         this->defineToStrMap[NETMSGTYPE_SetStorageTargetInfo] = "SetStorageTargetInfo";
         this->defineToStrMap[NETMSGTYPE_SetStorageTargetInfoResp] = "SetStorageTargetInfoResp";
         this->defineToStrMap[NETMSGTYPE_ListXAttr] = "ListXAttr";
         this->defineToStrMap[NETMSGTYPE_ListXAttrResp] = "ListXAttrResp";
         this->defineToStrMap[NETMSGTYPE_GetXAttr] = "GetXAttr";
         this->defineToStrMap[NETMSGTYPE_GetXAttrResp] = "GetXAttrResp";
         this->defineToStrMap[NETMSGTYPE_RemoveXAttr] = "RemoveXAttr";
         this->defineToStrMap[NETMSGTYPE_RemoveXAttrResp] = "RemoveXAttrResp";
         this->defineToStrMap[NETMSGTYPE_SetXAttr] = "SetXAttr";
         this->defineToStrMap[NETMSGTYPE_SetXAttrResp] = "SetXAttrResp";
         this->defineToStrMap[NETMSGTYPE_OpenFile] = "OpenFile";
         this->defineToStrMap[NETMSGTYPE_OpenFileResp] = "OpenFileResp";
         this->defineToStrMap[NETMSGTYPE_CloseFile] = "CloseFile";
         this->defineToStrMap[NETMSGTYPE_CloseFileResp] = "CloseFileResp";
         this->defineToStrMap[NETMSGTYPE_OpenLocalFile] = "OpenLocalFile";
         this->defineToStrMap[NETMSGTYPE_OpenLocalFileResp] = "OpenLocalFileResp";
         this->defineToStrMap[NETMSGTYPE_CloseChunkFile] = "CloseChunkFile";
         this->defineToStrMap[NETMSGTYPE_CloseChunkFileResp] = "CloseChunkFileResp";
         this->defineToStrMap[NETMSGTYPE_WriteLocalFile] = "WriteLocalFile";
         this->defineToStrMap[NETMSGTYPE_WriteLocalFileResp] = "WriteLocalFileResp";
         this->defineToStrMap[NETMSGTYPE_FSyncLocalFile] = "FSyncLocalFile";
         this->defineToStrMap[NETMSGTYPE_FSyncLocalFileResp] = "FSyncLocalFileResp";
         this->defineToStrMap[NETMSGTYPE_AcquireAppendLock] = "AcquireAppendLock";
         this->defineToStrMap[NETMSGTYPE_AcquireAppendLockResp] = "AcquireAppendLockResp";
         this->defineToStrMap[NETMSGTYPE_ReleaseAppendLock] = "ReleaseAppendLock";
         this->defineToStrMap[NETMSGTYPE_ReleaseAppendLockResp] = "ReleaseAppendLockResp";
         this->defineToStrMap[NETMSGTYPE_ReadLocalFileV2] = "ReadLocalFileV2";
         this->defineToStrMap[NETMSGTYPE_ReadLocalFileV2RespDummy] = "ReadLocalFileV2RespDummy";
         this->defineToStrMap[NETMSGTYPE_RefreshSession] = "RefreshSession";
         this->defineToStrMap[NETMSGTYPE_RefreshSessionResp] = "RefreshSessionResp";
         this->defineToStrMap[NETMSGTYPE_LockGranted] = "LockGranted";
         this->defineToStrMap[NETMSGTYPE_LockGrantedResp] = "LockGrantedResp";
         this->defineToStrMap[NETMSGTYPE_FLockEntry] = "FLockEntry";
         this->defineToStrMap[NETMSGTYPE_FLockEntryResp] = "FLockEntryResp";
         this->defineToStrMap[NETMSGTYPE_FLockRange] = "FLockRange";
         this->defineToStrMap[NETMSGTYPE_FLockRangeResp] = "FLockRangeResp";
         this->defineToStrMap[NETMSGTYPE_FLockAppend] = "FLockAppend";
         this->defineToStrMap[NETMSGTYPE_FLockAppendResp] = "FLockAppendResp";
         this->defineToStrMap[NETMSGTYPE_SetChannelDirect] = "SetChannelDirect";
         this->defineToStrMap[NETMSGTYPE_SetChannelDirectRespDummy] = "SetChannelDirectRespDummy";
         this->defineToStrMap[NETMSGTYPE_Ack] = "Ack";
         this->defineToStrMap[NETMSGTYPE_AckRespDummy] = "AckRespDummy";
         this->defineToStrMap[NETMSGTYPE_Dummy] = "Dummy";
         this->defineToStrMap[NETMSGTYPE_DummyRespDummy] = "DummyRespDummy";
         this->defineToStrMap[NETMSGTYPE_AuthenticateChannel] = "AuthenticateChannel";
         this->defineToStrMap[NETMSGTYPE_AuthenticateChannelRespDummy] = "AuthenticateChannelRespDummy";
         this->defineToStrMap[NETMSGTYPE_GenericResponse] = "GenericResponse";
         this->defineToStrMap[NETMSGTYPE_GenericResponseRespDummy] = "GenericResponseRespDummy";
         this->defineToStrMap[NETMSGTYPE_PeerInfo] = "PeerInfo";
         this->defineToStrMap[NETMSGTYPE_Log] = "Log";
         this->defineToStrMap[NETMSGTYPE_LogResp] = "LogResp";
         this->defineToStrMap[NETMSGTYPE_GetHostByName] = "GetHostByName";
         this->defineToStrMap[NETMSGTYPE_GetHostByNameResp] = "GetHostByNameResp";
         this->defineToStrMap[NETMSGTYPE_GetNodesFromRootMetaNode] = "GetNodesFromRootMetaNode";
         this->defineToStrMap[NETMSGTYPE_SendNodesList] = "SendNodesList";
         this->defineToStrMap[NETMSGTYPE_RequestMetaData] = "RequestMetaData";
         this->defineToStrMap[NETMSGTYPE_RequestStorageData] = "RequestStorageData";
         this->defineToStrMap[NETMSGTYPE_RequestMetaDataResp] = "RequestMetaDataResp";
         this->defineToStrMap[NETMSGTYPE_RequestStorageDataResp] = "RequestStorageDataResp";
         this->defineToStrMap[NETMSGTYPE_GetNodeInfo] = "GetNodeInfo";
         this->defineToStrMap[NETMSGTYPE_GetNodeInfoResp] = "GetNodeInfoResp";
         this->defineToStrMap[NETMSGTYPE_RetrieveDirEntries] = "RetrieveDirEntries";
         this->defineToStrMap[NETMSGTYPE_RetrieveDirEntriesResp] = "RetrieveDirEntriesResp";
         this->defineToStrMap[NETMSGTYPE_RetrieveInodes] = "RetrieveInodes";
         this->defineToStrMap[NETMSGTYPE_RetrieveInodesResp] = "RetrieveInodesResp";
         this->defineToStrMap[NETMSGTYPE_RetrieveChunks] = "RetrieveChunks";
         this->defineToStrMap[NETMSGTYPE_RetrieveChunksResp] = "RetrieveChunksResp";
         this->defineToStrMap[NETMSGTYPE_RetrieveFsIDs] = "RetrieveFsIDs";
         this->defineToStrMap[NETMSGTYPE_RetrieveFsIDsResp] = "RetrieveFsIDsResp";
         this->defineToStrMap[NETMSGTYPE_DeleteDirEntries] = "DeleteDirEntries";
         this->defineToStrMap[NETMSGTYPE_DeleteDirEntriesResp] = "DeleteDirEntriesResp";
         this->defineToStrMap[NETMSGTYPE_CreateDefDirInodes] = "CreateDefDirInodes";
         this->defineToStrMap[NETMSGTYPE_CreateDefDirInodesResp] = "CreateDefDirInodesResp";
         this->defineToStrMap[NETMSGTYPE_FixInodeOwnersInDentry] = "FixInodeOwnersInDentry";
         this->defineToStrMap[NETMSGTYPE_FixInodeOwnersInDentryResp] = "FixInodeOwnersInDentryResp";
         this->defineToStrMap[NETMSGTYPE_FixInodeOwners] = "FixInodeOwners";
         this->defineToStrMap[NETMSGTYPE_FixInodeOwnersResp] = "FixInodeOwnersResp";
         this->defineToStrMap[NETMSGTYPE_LinkToLostAndFound] = "LinkToLostAndFound";
         this->defineToStrMap[NETMSGTYPE_LinkToLostAndFoundResp] = "LinkToLostAndFoundResp";
         this->defineToStrMap[NETMSGTYPE_DeleteChunks] = "DeleteChunks";
         this->defineToStrMap[NETMSGTYPE_DeleteChunksResp] = "DeleteChunksResp";
         this->defineToStrMap[NETMSGTYPE_CreateEmptyContDirs] = "CreateEmptyContDirs";
         this->defineToStrMap[NETMSGTYPE_CreateEmptyContDirsResp] = "CreateEmptyContDirsResp";
         this->defineToStrMap[NETMSGTYPE_UpdateFileAttribs] = "UpdateFileAttribs";
         this->defineToStrMap[NETMSGTYPE_UpdateFileAttribsResp] = "UpdateFileAttribsResp";
         this->defineToStrMap[NETMSGTYPE_UpdateDirAttribs] = "UpdateDirAttribs";
         this->defineToStrMap[NETMSGTYPE_UpdateDirAttribsResp] = "UpdateDirAttribsResp";
         this->defineToStrMap[NETMSGTYPE_RemoveInodes] = "RemoveInodes";
         this->defineToStrMap[NETMSGTYPE_RemoveInodesResp] = "RemoveInodesResp";
         this->defineToStrMap[NETMSGTYPE_ChangeStripeTarget] = "ChangeStripeTarget";
         this->defineToStrMap[NETMSGTYPE_ChangeStripeTargetResp] = "ChangeStripeTargetResp";
         this->defineToStrMap[NETMSGTYPE_RecreateFsIDs] = "RecreateFsIDs";
         this->defineToStrMap[NETMSGTYPE_RecreateFsIDsResp] = "RecreateFsIDsResp";
         this->defineToStrMap[NETMSGTYPE_RecreateDentries] = "RecreateDentries";
         this->defineToStrMap[NETMSGTYPE_RecreateDentriesResp] = "RecreateDentriesResp";
         this->defineToStrMap[NETMSGTYPE_FsckModificationEvent] = "FsckModificationEvent";
         this->defineToStrMap[NETMSGTYPE_FsckSetEventLogging] = "FsckSetEventLogging";
         this->defineToStrMap[NETMSGTYPE_FsckSetEventLoggingResp] = "FsckSetEventLoggingResp";
         this->defineToStrMap[NETMSGTYPE_FetchFsckChunkList] = "FetchFsckChunkList";
         this->defineToStrMap[NETMSGTYPE_FetchFsckChunkListResp] = "FetchFsckChunkListResp";
         this->defineToStrMap[NETMSGTYPE_AdjustChunkPermissions] = "AdjustChunkPermissions";
         this->defineToStrMap[NETMSGTYPE_AdjustChunkPermissionsResp] = "AdjustChunkPermissionsResp";
         this->defineToStrMap[NETMSGTYPE_MoveChunkFile] = "MoveChunkFile";
         this->defineToStrMap[NETMSGTYPE_MoveChunkFileResp] = "MoveChunkFileResp";
         this->defineToStrMap[NETMSGTYPE_SetRootNodeID] = "SetRootNodeID";
         this->defineToStrMap[NETMSGTYPE_SetRootNodeIDResp] = "SetRootNodeIDResp";
      }

};


#endif /* NETMESSAGELOGHELPER_H_ */
