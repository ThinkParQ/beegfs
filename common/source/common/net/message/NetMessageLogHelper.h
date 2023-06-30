/*
 * Main purpose is to convert NetMessage defines into a string
 */

#ifndef NETMESSAGELOGHELPER_H_
#define NETMESSAGELOGHELPER_H_

#include <common/net/message/NetMessageTypes.h>

#include <string>

inline std::string netMessageTypeToStr(int type)
{
   /*
      Cases generated with shell command:

      awk '/NETMSGTYPE/ { printf("case %s: return \"%s (%i)\";\n", $2, substr($2, 12), $3); }' \
         common/net/message/NetMessageTypes.h
   */

   switch (type) {
      case NETMSGTYPE_Invalid: return "Invalid (0)";
      case NETMSGTYPE_RemoveNode: return "RemoveNode (1013)";
      case NETMSGTYPE_RemoveNodeResp: return "RemoveNodeResp (1014)";
      case NETMSGTYPE_GetNodes: return "GetNodes (1017)";
      case NETMSGTYPE_GetNodesResp: return "GetNodesResp (1018)";
      case NETMSGTYPE_HeartbeatRequest: return "HeartbeatRequest (1019)";
      case NETMSGTYPE_Heartbeat: return "Heartbeat (1020)";
      case NETMSGTYPE_GetNodeCapacityPools: return "GetNodeCapacityPools (1021)";
      case NETMSGTYPE_GetNodeCapacityPoolsResp: return "GetNodeCapacityPoolsResp (1022)";
      case NETMSGTYPE_MapTargets: return "MapTargets (1023)";
      case NETMSGTYPE_MapTargetsResp: return "MapTargetsResp (1024)";
      case NETMSGTYPE_GetTargetMappings: return "GetTargetMappings (1025)";
      case NETMSGTYPE_GetTargetMappingsResp: return "GetTargetMappingsResp (1026)";
      case NETMSGTYPE_UnmapTarget: return "UnmapTarget (1027)";
      case NETMSGTYPE_UnmapTargetResp: return "UnmapTargetResp (1028)";
      case NETMSGTYPE_GenericDebug: return "GenericDebug (1029)";
      case NETMSGTYPE_GenericDebugResp: return "GenericDebugResp (1030)";
      case NETMSGTYPE_GetClientStats: return "GetClientStats (1031)";
      case NETMSGTYPE_GetClientStatsResp: return "GetClientStatsResp (1032)";
      case NETMSGTYPE_RefreshCapacityPools: return "RefreshCapacityPools (1035)";
      case NETMSGTYPE_StorageBenchControlMsg: return "StorageBenchControlMsg (1037)";
      case NETMSGTYPE_StorageBenchControlMsgResp: return "StorageBenchControlMsgResp (1038)";
      case NETMSGTYPE_RegisterNode: return "RegisterNode (1039)";
      case NETMSGTYPE_RegisterNodeResp: return "RegisterNodeResp (1040)";
      case NETMSGTYPE_RegisterTarget: return "RegisterTarget (1041)";
      case NETMSGTYPE_RegisterTargetResp: return "RegisterTargetResp (1042)";
      case NETMSGTYPE_SetMirrorBuddyGroup: return "SetMirrorBuddyGroup (1045)";
      case NETMSGTYPE_SetMirrorBuddyGroupResp: return "SetMirrorBuddyGroupResp (1046)";
      case NETMSGTYPE_GetMirrorBuddyGroups: return "GetMirrorBuddyGroups (1047)";
      case NETMSGTYPE_GetMirrorBuddyGroupsResp: return "GetMirrorBuddyGroupsResp (1048)";
      case NETMSGTYPE_GetTargetStates: return "GetTargetStates (1049)";
      case NETMSGTYPE_GetTargetStatesResp: return "GetTargetStatesResp (1050)";
      case NETMSGTYPE_RefreshTargetStates: return "RefreshTargetStates (1051)";
      case NETMSGTYPE_GetStatesAndBuddyGroups: return "GetStatesAndBuddyGroups (1053)";
      case NETMSGTYPE_GetStatesAndBuddyGroupsResp: return "GetStatesAndBuddyGroupsResp (1054)";
      case NETMSGTYPE_SetTargetConsistencyStates: return "SetTargetConsistencyStates (1055)";
      case NETMSGTYPE_SetTargetConsistencyStatesResp: return "SetTargetConsistencyStatesResp (1056)";
      case NETMSGTYPE_ChangeTargetConsistencyStates: return "ChangeTargetConsistencyStates (1057)";
      case NETMSGTYPE_ChangeTargetConsistencyStatesResp: return "ChangeTargetConsistencyStatesResp (1058)";
      case NETMSGTYPE_PublishCapacities: return "PublishCapacities (1059)";
      case NETMSGTYPE_RemoveBuddyGroup: return "RemoveBuddyGroup (1060)";
      case NETMSGTYPE_RemoveBuddyGroupResp: return "RemoveBuddyGroupResp (1061)";
      case NETMSGTYPE_GetTargetConsistencyStates: return "GetTargetConsistencyStates (1062)";
      case NETMSGTYPE_GetTargetConsistencyStatesResp: return "GetTargetConsistencyStatesResp (1063)";
      case NETMSGTYPE_AddStoragePool: return "AddStoragePool (1064)";
      case NETMSGTYPE_AddStoragePoolResp: return "AddStoragePoolResp (1065)";
      case NETMSGTYPE_GetStoragePools: return "GetStoragePools (1066)";
      case NETMSGTYPE_GetStoragePoolsResp: return "GetStoragePoolsResp (1067)";
      case NETMSGTYPE_ModifyStoragePool: return "ModifyStoragePool (1068)";
      case NETMSGTYPE_ModifyStoragePoolResp: return "ModifyStoragePoolResp (1069)";
      case NETMSGTYPE_RefreshStoragePools: return "RefreshStoragePools (1070)";
      case NETMSGTYPE_RemoveStoragePool: return "RemoveStoragePool (1071)";
      case NETMSGTYPE_RemoveStoragePoolResp: return "RemoveStoragePoolResp (1072)";
      case NETMSGTYPE_MkDir: return "MkDir (2001)";
      case NETMSGTYPE_MkDirResp: return "MkDirResp (2002)";
      case NETMSGTYPE_RmDir: return "RmDir (2003)";
      case NETMSGTYPE_RmDirResp: return "RmDirResp (2004)";
      case NETMSGTYPE_MkFile: return "MkFile (2005)";
      case NETMSGTYPE_MkFileResp: return "MkFileResp (2006)";
      case NETMSGTYPE_UnlinkFile: return "UnlinkFile (2007)";
      case NETMSGTYPE_UnlinkFileResp: return "UnlinkFileResp (2008)";
      case NETMSGTYPE_UnlinkLocalFile: return "UnlinkLocalFile (2011)";
      case NETMSGTYPE_UnlinkLocalFileResp: return "UnlinkLocalFileResp (2012)";
      case NETMSGTYPE_Stat: return "Stat (2015)";
      case NETMSGTYPE_StatResp: return "StatResp (2016)";
      case NETMSGTYPE_GetChunkFileAttribs: return "GetChunkFileAttribs (2017)";
      case NETMSGTYPE_GetChunkFileAttribsResp: return "GetChunkFileAttribsResp (2018)";
      case NETMSGTYPE_TruncFile: return "TruncFile (2019)";
      case NETMSGTYPE_TruncFileResp: return "TruncFileResp (2020)";
      case NETMSGTYPE_TruncLocalFile: return "TruncLocalFile (2021)";
      case NETMSGTYPE_TruncLocalFileResp: return "TruncLocalFileResp (2022)";
      case NETMSGTYPE_Rename: return "Rename (2023)";
      case NETMSGTYPE_RenameResp: return "RenameResp (2024)";
      case NETMSGTYPE_SetAttr: return "SetAttr (2025)";
      case NETMSGTYPE_SetAttrResp: return "SetAttrResp (2026)";
      case NETMSGTYPE_ListDirFromOffset: return "ListDirFromOffset (2029)";
      case NETMSGTYPE_ListDirFromOffsetResp: return "ListDirFromOffsetResp (2030)";
      case NETMSGTYPE_StatStoragePath: return "StatStoragePath (2031)";
      case NETMSGTYPE_StatStoragePathResp: return "StatStoragePathResp (2032)";
      case NETMSGTYPE_SetLocalAttr: return "SetLocalAttr (2033)";
      case NETMSGTYPE_SetLocalAttrResp: return "SetLocalAttrResp (2034)";
      case NETMSGTYPE_FindOwner: return "FindOwner (2035)";
      case NETMSGTYPE_FindOwnerResp: return "FindOwnerResp (2036)";
      case NETMSGTYPE_MkLocalDir: return "MkLocalDir (2037)";
      case NETMSGTYPE_MkLocalDirResp: return "MkLocalDirResp (2038)";
      case NETMSGTYPE_RmLocalDir: return "RmLocalDir (2039)";
      case NETMSGTYPE_RmLocalDirResp: return "RmLocalDirResp (2040)";
      case NETMSGTYPE_MovingFileInsert: return "MovingFileInsert (2041)";
      case NETMSGTYPE_MovingFileInsertResp: return "MovingFileInsertResp (2042)";
      case NETMSGTYPE_MovingDirInsert: return "MovingDirInsert (2043)";
      case NETMSGTYPE_MovingDirInsertResp: return "MovingDirInsertResp (2044)";
      case NETMSGTYPE_GetEntryInfo: return "GetEntryInfo (2045)";
      case NETMSGTYPE_GetEntryInfoResp: return "GetEntryInfoResp (2046)";
      case NETMSGTYPE_SetDirPattern: return "SetDirPattern (2047)";
      case NETMSGTYPE_SetDirPatternResp: return "SetDirPatternResp (2048)";
      case NETMSGTYPE_GetHighResStats: return "GetHighResStats (2051)";
      case NETMSGTYPE_GetHighResStatsResp: return "GetHighResStatsResp (2052)";
      case NETMSGTYPE_MkFileWithPattern: return "MkFileWithPattern (2053)";
      case NETMSGTYPE_MkFileWithPatternResp: return "MkFileWithPatternResp (2054)";
      case NETMSGTYPE_RefreshEntryInfo: return "RefreshEntryInfo (2055)";
      case NETMSGTYPE_RefreshEntryInfoResp: return "RefreshEntryInfoResp (2056)";
      case NETMSGTYPE_RmDirEntry: return "RmDirEntry (2057)";
      case NETMSGTYPE_RmDirEntryResp: return "RmDirEntryResp (2058)";
      case NETMSGTYPE_LookupIntent: return "LookupIntent (2059)";
      case NETMSGTYPE_LookupIntentResp: return "LookupIntentResp (2060)";
      case NETMSGTYPE_FindLinkOwner: return "FindLinkOwner (2063)";
      case NETMSGTYPE_FindLinkOwnerResp: return "FindLinkOwnerResp (2064)";
      case NETMSGTYPE_MirrorMetadata: return "MirrorMetadata (2067)";
      case NETMSGTYPE_MirrorMetadataResp: return "MirrorMetadataResp (2068)";
      case NETMSGTYPE_SetMetadataMirroring: return "SetMetadataMirroring (2069)";
      case NETMSGTYPE_SetMetadataMirroringResp: return "SetMetadataMirroringResp (2070)";
      case NETMSGTYPE_Hardlink: return "Hardlink (2071)";
      case NETMSGTYPE_HardlinkResp: return "HardlinkResp (2072)";
      case NETMSGTYPE_SetQuota: return "SetQuota (2075)";
      case NETMSGTYPE_SetQuotaResp: return "SetQuotaResp (2076)";
      case NETMSGTYPE_SetExceededQuota: return "SetExceededQuota (2077)";
      case NETMSGTYPE_SetExceededQuotaResp: return "SetExceededQuotaResp (2078)";
      case NETMSGTYPE_RequestExceededQuota: return "RequestExceededQuota (2079)";
      case NETMSGTYPE_RequestExceededQuotaResp: return "RequestExceededQuotaResp (2080)";
      case NETMSGTYPE_UpdateDirParent: return "UpdateDirParent (2081)";
      case NETMSGTYPE_UpdateDirParentResp: return "UpdateDirParentResp (2082)";
      case NETMSGTYPE_ResyncLocalFile: return "ResyncLocalFile (2083)";
      case NETMSGTYPE_ResyncLocalFileResp: return "ResyncLocalFileResp (2084)";
      case NETMSGTYPE_StartStorageTargetResync: return "StartStorageTargetResync (2085)";
      case NETMSGTYPE_StartStorageTargetResyncResp: return "StartStorageTargetResyncResp (2086)";
      case NETMSGTYPE_StorageResyncStarted: return "StorageResyncStarted (2087)";
      case NETMSGTYPE_StorageResyncStartedResp: return "StorageResyncStartedResp (2088)";
      case NETMSGTYPE_ListChunkDirIncremental: return "ListChunkDirIncremental (2089)";
      case NETMSGTYPE_ListChunkDirIncrementalResp: return "ListChunkDirIncrementalResp (2090)";
      case NETMSGTYPE_RmChunkPaths: return "RmChunkPaths (2091)";
      case NETMSGTYPE_RmChunkPathsResp: return "RmChunkPathsResp (2092)";
      case NETMSGTYPE_GetStorageResyncStats: return "GetStorageResyncStats (2093)";
      case NETMSGTYPE_GetStorageResyncStatsResp: return "GetStorageResyncStatsResp (2094)";
      case NETMSGTYPE_SetLastBuddyCommOverride: return "SetLastBuddyCommOverride (2095)";
      case NETMSGTYPE_SetLastBuddyCommOverrideResp: return "SetLastBuddyCommOverrideResp (2096)";
      case NETMSGTYPE_GetQuotaInfo: return "GetQuotaInfo (2097)";
      case NETMSGTYPE_GetQuotaInfoResp: return "GetQuotaInfoResp (2098)";
      case NETMSGTYPE_SetStorageTargetInfo: return "SetStorageTargetInfo (2099)";
      case NETMSGTYPE_SetStorageTargetInfoResp: return "SetStorageTargetInfoResp (2100)";
      case NETMSGTYPE_ListXAttr: return "ListXAttr (2101)";
      case NETMSGTYPE_ListXAttrResp: return "ListXAttrResp (2102)";
      case NETMSGTYPE_GetXAttr: return "GetXAttr (2103)";
      case NETMSGTYPE_GetXAttrResp: return "GetXAttrResp (2104)";
      case NETMSGTYPE_RemoveXAttr: return "RemoveXAttr (2105)";
      case NETMSGTYPE_RemoveXAttrResp: return "RemoveXAttrResp (2106)";
      case NETMSGTYPE_SetXAttr: return "SetXAttr (2107)";
      case NETMSGTYPE_SetXAttrResp: return "SetXAttrResp (2108)";
      case NETMSGTYPE_GetDefaultQuota: return "GetDefaultQuota (2109)";
      case NETMSGTYPE_GetDefaultQuotaResp: return "GetDefaultQuotaResp (2110)";
      case NETMSGTYPE_SetDefaultQuota: return "SetDefaultQuota (2111)";
      case NETMSGTYPE_SetDefaultQuotaResp: return "SetDefaultQuotaResp (2112)";
      case NETMSGTYPE_ResyncSessionStore: return "ResyncSessionStore (2113)";
      case NETMSGTYPE_ResyncSessionStoreResp: return "ResyncSessionStoreResp (2114)";
      case NETMSGTYPE_ResyncRawInodes: return "ResyncRawInodes (2115)";
      case NETMSGTYPE_ResyncRawInodesResp: return "ResyncRawInodesResp (2116)";
      case NETMSGTYPE_GetMetaResyncStats: return "GetMetaResyncStats (2117)";
      case NETMSGTYPE_GetMetaResyncStatsResp: return "GetMetaResyncStatsResp (2118)";
      case NETMSGTYPE_MoveFileInode: return "MoveFileInode (2119)";
      case NETMSGTYPE_MoveFileInodeResp: return "MoveFileInodeResp(2120)";
      case NETMSGTYPE_UnlinkLocalFileInode: return "UnlinkLocalFileInode (2121)";
      case NETMSGTYPE_UnlinkLocalFileInodeResp: return "UnlinkLocalFileInodeResp (2122)";
      case NETMSGTYPE_OpenFile: return "OpenFile (3001)";
      case NETMSGTYPE_OpenFileResp: return "OpenFileResp (3002)";
      case NETMSGTYPE_CloseFile: return "CloseFile (3003)";
      case NETMSGTYPE_CloseFileResp: return "CloseFileResp (3004)";
      case NETMSGTYPE_OpenLocalFile: return "OpenLocalFile (3005)";
      case NETMSGTYPE_OpenLocalFileResp: return "OpenLocalFileResp (3006)";
      case NETMSGTYPE_CloseChunkFile: return "CloseChunkFile (3007)";
      case NETMSGTYPE_CloseChunkFileResp: return "CloseChunkFileResp (3008)";
      case NETMSGTYPE_WriteLocalFile: return "WriteLocalFile (3009)";
      case NETMSGTYPE_WriteLocalFileResp: return "WriteLocalFileResp (3010)";
      case NETMSGTYPE_FSyncLocalFile: return "FSyncLocalFile (3013)";
      case NETMSGTYPE_FSyncLocalFileResp: return "FSyncLocalFileResp (3014)";
      case NETMSGTYPE_ReadLocalFileV2: return "ReadLocalFileV2 (3019)";
      case NETMSGTYPE_RefreshSession: return "RefreshSession (3021)";
      case NETMSGTYPE_RefreshSessionResp: return "RefreshSessionResp (3022)";
      case NETMSGTYPE_LockGranted: return "LockGranted (3023)";
      case NETMSGTYPE_FLockEntry: return "FLockEntry (3025)";
      case NETMSGTYPE_FLockEntryResp: return "FLockEntryResp (3026)";
      case NETMSGTYPE_FLockRange: return "FLockRange (3027)";
      case NETMSGTYPE_FLockRangeResp: return "FLockRangeResp (3028)";
      case NETMSGTYPE_FLockAppend: return "FLockAppend (3029)";
      case NETMSGTYPE_FLockAppendResp: return "FLockAppendResp (3030)";
      case NETMSGTYPE_AckNotify: return "AckNotify (3031)";
      case NETMSGTYPE_AckNotifyResp: return "AckNotifyResp (3032)";
      case NETMSGTYPE_BumpFileVersion: return "BumpFileVersion (3033)";
      case NETMSGTYPE_BumpFileVersionResp: return "BumpFileVersionResp (3034)";
      case NETMSGTYPE_GetFileVersion: return "GetFileVersion (3035)";
      case NETMSGTYPE_GetFileVersionResp: return "GetFileVersionResp (3036)";
#ifdef BEEGFS_NVFS
      case NETMSGTYPE_WriteLocalFileRDMA: return "WriteLocalFileRDMA (3037)";
      case NETMSGTYPE_WriteLocalFileRDMAResp: return "WriteLocalFileRDMAResp (3038)";
      case NETMSGTYPE_ReadLocalFileRDMA: return "ReadLocalFileRDMA (3039)";
      case NETMSGTYPE_ReadLocalFileRDMAResp: return "ReadLocalFileRDMAResp (3040)";
#endif /* BEEGFS_NVFS */
      case NETMSGTYPE_SetChannelDirect: return "SetChannelDirect (4001)";
      case NETMSGTYPE_Ack: return "Ack (4003)";
      case NETMSGTYPE_Dummy: return "Dummy (4005)";
      case NETMSGTYPE_AuthenticateChannel: return "AuthenticateChannel (4007)";
      case NETMSGTYPE_GenericResponse: return "GenericResponse (4009)";
      case NETMSGTYPE_PeerInfo: return "PeerInfo (4011)";
      case NETMSGTYPE_Log: return "Log (5001)";
      case NETMSGTYPE_LogResp: return "LogResp (5002)";
      case NETMSGTYPE_GetHostByName: return "GetHostByName (5003)";
      case NETMSGTYPE_GetHostByNameResp: return "GetHostByNameResp (5004)";
      case NETMSGTYPE_GetNodesFromRootMetaNode: return "GetNodesFromRootMetaNode (6001)";
      case NETMSGTYPE_SendNodesList: return "SendNodesList (6002)";
      case NETMSGTYPE_RequestMetaData: return "RequestMetaData (6003)";
      case NETMSGTYPE_RequestStorageData: return "RequestStorageData (6004)";
      case NETMSGTYPE_RequestMetaDataResp: return "RequestMetaDataResp (6005)";
      case NETMSGTYPE_RequestStorageDataResp: return "RequestStorageDataResp (6006)";
      case NETMSGTYPE_RetrieveDirEntries: return "RetrieveDirEntries (7001)";
      case NETMSGTYPE_RetrieveDirEntriesResp: return "RetrieveDirEntriesResp (7002)";
      case NETMSGTYPE_RetrieveInodes: return "RetrieveInodes (7003)";
      case NETMSGTYPE_RetrieveInodesResp: return "RetrieveInodesResp (7004)";
      case NETMSGTYPE_RetrieveChunks: return "RetrieveChunks (7005)";
      case NETMSGTYPE_RetrieveChunksResp: return "RetrieveChunksResp (7006)";
      case NETMSGTYPE_RetrieveFsIDs: return "RetrieveFsIDs (7007)";
      case NETMSGTYPE_RetrieveFsIDsResp: return "RetrieveFsIDsResp (7008)";
      case NETMSGTYPE_DeleteDirEntries: return "DeleteDirEntries (7009)";
      case NETMSGTYPE_DeleteDirEntriesResp: return "DeleteDirEntriesResp (7010)";
      case NETMSGTYPE_CreateDefDirInodes: return "CreateDefDirInodes (7011)";
      case NETMSGTYPE_CreateDefDirInodesResp: return "CreateDefDirInodesResp (7012)";
      case NETMSGTYPE_FixInodeOwnersInDentry: return "FixInodeOwnersInDentry (7013)";
      case NETMSGTYPE_FixInodeOwnersInDentryResp: return "FixInodeOwnersInDentryResp (7014)";
      case NETMSGTYPE_FixInodeOwners: return "FixInodeOwners (7015)";
      case NETMSGTYPE_FixInodeOwnersResp: return "FixInodeOwnersResp (7016)";
      case NETMSGTYPE_LinkToLostAndFound: return "LinkToLostAndFound (7017)";
      case NETMSGTYPE_LinkToLostAndFoundResp: return "LinkToLostAndFoundResp (7018)";
      case NETMSGTYPE_DeleteChunks: return "DeleteChunks (7019)";
      case NETMSGTYPE_DeleteChunksResp: return "DeleteChunksResp (7020)";
      case NETMSGTYPE_CreateEmptyContDirs: return "CreateEmptyContDirs (7021)";
      case NETMSGTYPE_CreateEmptyContDirsResp: return "CreateEmptyContDirsResp (7022)";
      case NETMSGTYPE_UpdateFileAttribs: return "UpdateFileAttribs (7023)";
      case NETMSGTYPE_UpdateFileAttribsResp: return "UpdateFileAttribsResp (7024)";
      case NETMSGTYPE_UpdateDirAttribs: return "UpdateDirAttribs (7025)";
      case NETMSGTYPE_UpdateDirAttribsResp: return "UpdateDirAttribsResp (7026)";
      case NETMSGTYPE_RemoveInodes: return "RemoveInodes (7027)";
      case NETMSGTYPE_RemoveInodesResp: return "RemoveInodesResp (7028)";
      case NETMSGTYPE_RecreateFsIDs: return "RecreateFsIDs (7031)";
      case NETMSGTYPE_RecreateFsIDsResp: return "RecreateFsIDsResp (7032)";
      case NETMSGTYPE_RecreateDentries: return "RecreateDentries (7033)";
      case NETMSGTYPE_RecreateDentriesResp: return "RecreateDentriesResp (7034)";
      case NETMSGTYPE_FsckModificationEvent: return "FsckModificationEvent (7035)";
      case NETMSGTYPE_FsckSetEventLogging: return "FsckSetEventLogging (7036)";
      case NETMSGTYPE_FsckSetEventLoggingResp: return "FsckSetEventLoggingResp (7037)";
      case NETMSGTYPE_FetchFsckChunkList: return "FetchFsckChunkList (7038)";
      case NETMSGTYPE_FetchFsckChunkListResp: return "FetchFsckChunkListResp (7039)";
      case NETMSGTYPE_AdjustChunkPermissions: return "AdjustChunkPermissions (7040)";
      case NETMSGTYPE_AdjustChunkPermissionsResp: return "AdjustChunkPermissionsResp (7041)";
      case NETMSGTYPE_MoveChunkFile: return "MoveChunkFile (7042)";
      case NETMSGTYPE_MoveChunkFileResp: return "MoveChunkFileResp (7043)";
      case NETMSGTYPE_CheckAndRepairDupInode: return "CheckAndRepairDupInode (7044)";
      case NETMSGTYPE_CheckAndRepairDupInodeResp: return "CheckAndRepairDupInodeResp (7045)";
   }

   return "unknown (" + std::to_string(type) + ")";
}

#endif /* NETMESSAGELOGHELPER_H_ */
