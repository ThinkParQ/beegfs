#ifndef NETMESSAGETYPES_H_
#define NETMESSAGETYPES_H_

/* This file MUST be kept in sync with the corresponding client file!
 * See fhgfs_client/source/closed/common/net/message/NetMessageTypes.h
 *
 * Also do not forget to add net NetMessages to 'class NetMsgStrMapping'
 */

// invalid messages
#define NETMSGTYPE_Invalid                            0

// nodes messages
#define NETMSGTYPE_RemoveNode                      1013
#define NETMSGTYPE_RemoveNodeResp                  1014
#define NETMSGTYPE_GetNodes                        1017
#define NETMSGTYPE_GetNodesResp                    1018
#define NETMSGTYPE_HeartbeatRequest                1019
#define NETMSGTYPE_Heartbeat                       1020
#define NETMSGTYPE_GetNodeCapacityPools            1021
#define NETMSGTYPE_GetNodeCapacityPoolsResp        1022
#define NETMSGTYPE_MapTargets                      1023
#define NETMSGTYPE_MapTargetsResp                  1024
#define NETMSGTYPE_GetTargetMappings               1025
#define NETMSGTYPE_GetTargetMappingsResp           1026
#define NETMSGTYPE_UnmapTarget                     1027
#define NETMSGTYPE_UnmapTargetResp                 1028
#define NETMSGTYPE_GenericDebug                    1029
#define NETMSGTYPE_GenericDebugResp                1030
#define NETMSGTYPE_GetClientStats                  1031
#define NETMSGTYPE_GetClientStatsResp              1032
#define NETMSGTYPE_RefresherControl                1033
#define NETMSGTYPE_RefresherControlResp            1034
#define NETMSGTYPE_RefreshCapacityPools            1035
#define NETMSGTYPE_RefreshCapacityPoolsResp        1036 // this msg only has an ack response
#define NETMSGTYPE_StorageBenchControlMsg          1037
#define NETMSGTYPE_StorageBenchControlMsgResp      1038
#define NETMSGTYPE_RegisterNode                    1039
#define NETMSGTYPE_RegisterNodeResp                1040
#define NETMSGTYPE_RegisterTarget                  1041
#define NETMSGTYPE_RegisterTargetResp              1042

// storage messages
#define NETMSGTYPE_MkDir                           2001
#define NETMSGTYPE_MkDirResp                       2002
#define NETMSGTYPE_RmDir                           2003
#define NETMSGTYPE_RmDirResp                       2004
#define NETMSGTYPE_MkFile                          2005
#define NETMSGTYPE_MkFileResp                      2006
#define NETMSGTYPE_UnlinkFile                      2007
#define NETMSGTYPE_UnlinkFileResp                  2008
#define NETMSGTYPE_MkLocalFile                     2009
#define NETMSGTYPE_MkLocalFileResp                 2010
#define NETMSGTYPE_UnlinkLocalFile                 2011
#define NETMSGTYPE_UnlinkLocalFileResp             2012
#define NETMSGTYPE_Stat                            2015
#define NETMSGTYPE_StatResp                        2016
#define NETMSGTYPE_GetChunkFileAttribs             2017
#define NETMSGTYPE_GetChunkFileAttribsResp         2018
#define NETMSGTYPE_TruncFile                       2019
#define NETMSGTYPE_TruncFileResp                   2020
#define NETMSGTYPE_TruncLocalFile                  2021
#define NETMSGTYPE_TruncLocalFileResp              2022
#define NETMSGTYPE_Rename                          2023
#define NETMSGTYPE_RenameResp                      2024
#define NETMSGTYPE_SetAttr                         2025
#define NETMSGTYPE_SetAttrResp                     2026
#define NETMSGTYPE_ListDirFromOffset               2029
#define NETMSGTYPE_ListDirFromOffsetResp           2030
#define NETMSGTYPE_StatStoragePath                 2031
#define NETMSGTYPE_StatStoragePathResp             2032
#define NETMSGTYPE_SetLocalAttr                    2033
#define NETMSGTYPE_SetLocalAttrResp                2034
#define NETMSGTYPE_FindOwner                       2035
#define NETMSGTYPE_FindOwnerResp                   2036
#define NETMSGTYPE_MkLocalDir                      2037
#define NETMSGTYPE_MkLocalDirResp                  2038
#define NETMSGTYPE_RmLocalDir                      2039
#define NETMSGTYPE_RmLocalDirResp                  2040
#define NETMSGTYPE_MovingFileInsert                2041
#define NETMSGTYPE_MovingFileInsertResp            2042
#define NETMSGTYPE_MovingDirInsert                 2043
#define NETMSGTYPE_MovingDirInsertResp             2044
#define NETMSGTYPE_GetEntryInfo                    2045
#define NETMSGTYPE_GetEntryInfoResp                2046
#define NETMSGTYPE_SetDirPattern                   2047
#define NETMSGTYPE_SetDirPatternResp               2048
#define NETMSGTYPE_GetHighResStats                 2051
#define NETMSGTYPE_GetHighResStatsResp             2052
#define NETMSGTYPE_MkFileWithPattern               2053
#define NETMSGTYPE_MkFileWithPatternResp           2054
#define NETMSGTYPE_RefreshEntryInfo                2055
#define NETMSGTYPE_RefreshEntryInfoResp            2056
#define NETMSGTYPE_RmDirEntry                      2057
#define NETMSGTYPE_RmDirEntryResp                  2058
#define NETMSGTYPE_LookupIntent                    2059
#define NETMSGTYPE_LookupIntentResp                2060
#define NETMSGTYPE_FindEntryname                   2061
#define NETMSGTYPE_FindEntrynameResp               2062
#define NETMSGTYPE_FindLinkOwner                   2063
#define NETMSGTYPE_FindLinkOwnerResp               2064

// session messages
#define NETMSGTYPE_OpenFile                        3001
#define NETMSGTYPE_OpenFileResp                    3002
#define NETMSGTYPE_CloseFile                       3003
#define NETMSGTYPE_CloseFileResp                   3004
#define NETMSGTYPE_OpenLocalFile                   3005
#define NETMSGTYPE_OpenLocalFileResp               3006
#define NETMSGTYPE_CloseChunkFile                  3007
#define NETMSGTYPE_CloseChunkFileResp              3008
#define NETMSGTYPE_WriteLocalFile                  3009
#define NETMSGTYPE_WriteLocalFileResp              3010
#define NETMSGTYPE_FSyncLocalFile                  3013
#define NETMSGTYPE_FSyncLocalFileResp              3014
#define NETMSGTYPE_AcquireAppendLock               3015
#define NETMSGTYPE_AcquireAppendLockResp           3016
#define NETMSGTYPE_ReleaseAppendLock               3017
#define NETMSGTYPE_ReleaseAppendLockResp           3018
#define NETMSGTYPE_ReadLocalFileV2                 3019
#define NETMSGTYPE_ReadLocalFileV2RespDummy        3020 // this msg requires no such response
#define NETMSGTYPE_RefreshSession                  3021
#define NETMSGTYPE_RefreshSessionResp              3022
#define NETMSGTYPE_LockGranted                     3023
#define NETMSGTYPE_LockGrantedResp                 3024 // this msg only has an ack response
#define NETMSGTYPE_FLockEntry                      3025
#define NETMSGTYPE_FLockEntryResp                  3026
#define NETMSGTYPE_FLockRange                      3027
#define NETMSGTYPE_FLockRangeResp                  3028

// control messages
#define NETMSGTYPE_SetChannelDirect                4001
#define NETMSGTYPE_SetChannelDirectRespDummy       4002 // this msg requires no response
#define NETMSGTYPE_Ack                             4003
#define NETMSGTYPE_AckRespDummy                    4004 // this msg requires no response
#define NETMSGTYPE_Dummy                           4005
#define NETMSGTYPE_DummyRespDummy                  4006 // this msg requires no response

// helperd messages
#define NETMSGTYPE_Log                             5001
#define NETMSGTYPE_LogResp                         5002
#define NETMSGTYPE_GetHostByName                   5003
#define NETMSGTYPE_GetHostByNameResp               5004

// admon messages
#define NETMSGTYPE_GetNodesFromRootMetaNode        6001
#define NETMSGTYPE_SendNodesList        	         6002
#define NETMSGTYPE_RequestMetaData                 6003
#define NETMSGTYPE_RequestStorageData              6004
#define NETMSGTYPE_RequestMetaDataResp             6005
#define NETMSGTYPE_RequestStorageDataResp          6006
#define NETMSGTYPE_GetNodeInfo					      6007
#define NETMSGTYPE_GetNodeInfoResp					   6008

// fsck messages
#define NETMSGTYPE_RetrieveDirEntries              7001
#define NETMSGTYPE_RetrieveDirEntriesResp          7002
#define NETMSGTYPE_RetrieveInodes                  7003
#define NETMSGTYPE_RetrieveInodesResp              7004
#define NETMSGTYPE_RetrieveChunks                  7005
#define NETMSGTYPE_RetrieveChunksResp              7006
#define NETMSGTYPE_RetrieveFsIDs                   7007
#define NETMSGTYPE_RetrieveFsIDsResp               7008
#define NETMSGTYPE_DeleteDirEntries                7009
#define NETMSGTYPE_DeleteDirEntriesResp            7010
#define NETMSGTYPE_CreateDefDirInodes              7011
#define NETMSGTYPE_CreateDefDirInodesResp          7012
#define NETMSGTYPE_FixInodeOwnersInDentry          7013
#define NETMSGTYPE_FixInodeOwnersInDentryResp      7014
#define NETMSGTYPE_FixInodeOwners                  7015
#define NETMSGTYPE_FixInodeOwnersResp              7016
#define NETMSGTYPE_LinkToLostAndFound              7017
#define NETMSGTYPE_LinkToLostAndFoundResp          7018
#define NETMSGTYPE_DeleteChunks                    7019
#define NETMSGTYPE_DeleteChunksResp                7020
#define NETMSGTYPE_CreateEmptyContDirs             7021
#define NETMSGTYPE_CreateEmptyContDirsResp         7022
#define NETMSGTYPE_UpdateFileAttribs               7023
#define NETMSGTYPE_UpdateFileAttribsResp           7024
#define NETMSGTYPE_UpdateDirAttribs                7025
#define NETMSGTYPE_UpdateDirAttribsResp            7026
#define NETMSGTYPE_RemoveInode                     7027
#define NETMSGTYPE_RemoveInodeResp                 7028
#define NETMSGTYPE_ChangeStripeTarget              7029
#define NETMSGTYPE_ChangeStripeTargetResp          7030
#define NETMSGTYPE_RecreateFsIDs                   7031
#define NETMSGTYPE_RecreateFsIDsResp               7032
#define NETMSGTYPE_RecreateDentries                7033
#define NETMSGTYPE_RecreateDentriesResp            7034

#define NETMSGTYPE_SetRootMDS                      7117
#define NETMSGTYPE_SetRootMDSResp                  7118

#endif /*NETMESSAGETYPES_H_*/
