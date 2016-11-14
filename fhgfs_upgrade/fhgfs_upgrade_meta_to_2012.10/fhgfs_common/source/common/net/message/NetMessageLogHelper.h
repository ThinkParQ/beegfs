/*
 * Main purpose is to convert NetMessage defines into a string
 */

#ifndef NETMESSAGELOGHELPER_H_
#define NETMESSAGELOGHELPER_H_

#include <common/net/message/NetMessageTypes.h>

typedef std::map<int, std::string> DefineToStrMap;
typedef DefineToStrMap::iterator   DefineToStrMapIter;

/**
 * Convert #defines or enums to strings
 *
 * See 'class NetMsgStrMapping' how to do an automatic mapping from defines (shell script)
 *
 * FIXME Bernd/Sven: Move this class into a generic file. Let
 *                   class MetaOpMapping and class StorageOpMapping should use it.
 */
class DefineToStrMapping
{
   protected:
      DefineToStrMap defineToStrMap;

   public:

      /**
       * Find the given define/enum in the map and return the corresponding string.
       * If it cannot be found,  there is likely a bug somewhere,
       * but we simply return the number as string then.
       */
      std::string defineToStr(int define)
      {
         DefineToStrMapIter iter = this->defineToStrMap.find(define);
         if (iter == this->defineToStrMap.end() )
            return StringTk::intToStr(define);

         return iter->second;
      }
};

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

            OLDIFS="$IFS"; \
            IFS=$'\n'; \
            for line in \
               `grep "define NETMSGTYPE_"  fhgfs_common/source/common/net/message/NetMessageTypes.h`;
               do
               echo $line \
                  | awk '{print "this->defineToStrMap[" $2 "] = \"" $2 "\";"   }' \
                  | sed 's/= "NETMSGTYPE_/= "/' \
                     >>/tmp/netmsg_to_str.txt 2>&1; \
               done; \
            IFS="$OLDIFS"
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
         this->defineToStrMap[NETMSGTYPE_SetChannelDirect] = "SetChannelDirect";
         this->defineToStrMap[NETMSGTYPE_SetChannelDirectRespDummy] = "SetChannelDirectRespDummy";
         this->defineToStrMap[NETMSGTYPE_Ack] = "Ack";
         this->defineToStrMap[NETMSGTYPE_AckRespDummy] = "AckRespDummy";
         this->defineToStrMap[NETMSGTYPE_Dummy] = "Dummy";
         this->defineToStrMap[NETMSGTYPE_DummyRespDummy] = "DummyRespDummy";
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
         this->defineToStrMap[NETMSGTYPE_RemoveInode] = "RemoveInode";
         this->defineToStrMap[NETMSGTYPE_RemoveInodeResp] = "RemoveInodeResp";
         this->defineToStrMap[NETMSGTYPE_ChangeStripeTarget] = "ChangeStripeTarget";
         this->defineToStrMap[NETMSGTYPE_ChangeStripeTargetResp] = "ChangeStripeTargetResp";
         this->defineToStrMap[NETMSGTYPE_RecreateFsIDs] = "RecreateFsIDs";
         this->defineToStrMap[NETMSGTYPE_RecreateFsIDsResp] = "RecreateFsIDsResp";
         this->defineToStrMap[NETMSGTYPE_RecreateDentries] = "RecreateDentries";
         this->defineToStrMap[NETMSGTYPE_RecreateDentriesResp] = "RecreateDentriesResp";
         this->defineToStrMap[NETMSGTYPE_SetRootMDS] = "SetRootMDS";
         this->defineToStrMap[NETMSGTYPE_SetRootMDSResp] = "SetRootMDSResp";
      }

};


#endif /* NETMESSAGELOGHELPER_H_ */
