// control messages
#include "control/AckMsgEx.h"
#include "control/SetChannelDirectMsgEx.h"
// nodes messages
#include "nodes/GenericDebugMsgEx.h"
#include <net/message/nodes/GetClientStatsMsgEx.h>
#include "nodes/GetNodeCapacityPoolsMsgEx.h"
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include "nodes/GetNodesMsgEx.h"
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include "nodes/GetTargetMappingsMsgEx.h"
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include "nodes/HeartbeatMsgEx.h"
#include "nodes/HeartbeatRequestMsgEx.h"
#include "nodes/MapTargetsMsgEx.h"
#include "nodes/RefreshCapacityPoolsMsgEx.h"
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include "nodes/RemoveNodeMsgEx.h"
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include "nodes/RefresherControlMsgEx.h"
// storage messages
#include "storage/lookup/FindOwnerMsgEx.h"
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include "storage/listing/ListDirFromOffsetMsgEx.h"
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include "storage/creating/MkDirMsgEx.h"
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include "storage/creating/MkFileMsgEx.h"
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include "storage/creating/MkFileWithPatternMsgEx.h"
#include <common/net/message/storage/creating/MkFileWithPatternRespMsg.h>
#include "storage/creating/MkLocalDirMsgEx.h"
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>
#include <common/net/message/storage/creating/MkLocalFileRespMsg.h>
#include "storage/creating/RmDirMsgEx.h"
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include "storage/creating/RmLocalDirMsgEx.h"
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include "storage/creating/RmDirEntryMsgEx.h"
#include "storage/moving/MovingDirInsertMsgEx.h"
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include "storage/moving/MovingFileInsertMsgEx.h"
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include "storage/moving/RenameV2MsgEx.h"
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include "storage/attribs/GetEntryInfoMsgEx.h"
#include "storage/lookup/LookupIntentMsgEx.h"
#include "storage/attribs/SetAttrMsgEx.h"
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include "storage/attribs/SetDirPatternMsgEx.h"
#include "storage/attribs/StatMsgEx.h"
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include "storage/StatStoragePathMsgEx.h"
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include "storage/TruncFileMsgEx.h"
#include <common/net/message/storage/TruncFileRespMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include "storage/creating/UnlinkFileMsgEx.h"
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include "storage/GetHighResStatsMsgEx.h"
#include "storage/attribs/RefreshEntryInfoMsgEx.h"
#include "storage/lookup/FindEntrynameMsgEx.h"
#include "storage/lookup/FindLinkOwnerMsgEx.h"

// session messages
#include "session/locking/AcquireAppendLockMsgEx.h"
#include <common/net/message/session/locking/AcquireAppendLockRespMsg.h>
#include "session/locking/ReleaseAppendLockMsgEx.h"
#include <common/net/message/session/locking/ReleaseAppendLockRespMsg.h>
#include <net/message/session/locking/FLockEntryMsgEx.h>
#include <net/message/session/locking/FLockRangeMsgEx.h>
#include "session/opening/CloseFileMsgEx.h"
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/net/message/session/FSyncLocalFileRespMsg.h>
#include "session/opening/OpenFileMsgEx.h"
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
// admon messages
#include "admon/RequestMetaDataMsgEx.h"
#include <net/message/admon/GetNodeInfoMsgEx.h>
// fsck messages
#include <common/net/message/fsck/RemoveInodeRespMsg.h>
#include <net/message/fsck/ChangeStripeTargetMsgEx.h>
#include <net/message/fsck/CreateDefDirInodesMsgEx.h>
#include <net/message/fsck/CreateEmptyContDirsMsgEx.h>
#include <net/message/fsck/DeleteDirEntriesMsgEx.h>
#include <net/message/fsck/FixInodeOwnersMsgEx.h>
#include <net/message/fsck/FixInodeOwnersInDentryMsgEx.h>
#include <net/message/fsck/LinkToLostAndFoundMsgEx.h>
#include <net/message/fsck/RecreateFsIDsMsgEx.h>
#include <net/message/fsck/RecreateDentriesMsgEx.h>
#include <net/message/fsck/RemoveInodeMsgEx.h>
#include <net/message/fsck/RetrieveDirEntriesMsgEx.h>
#include <net/message/fsck/RetrieveInodesMsgEx.h>
#include <net/message/fsck/RetrieveFsIDsMsgEx.h>
#include <net/message/fsck/UpdateDirAttribsMsgEx.h>
#include <net/message/fsck/UpdateFileAttribsMsgEx.h>

#include <common/net/message/SimpleMsg.h>
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
      // control messages
      case NETMSGTYPE_Ack: { msg = new AckMsgEx(); } break;
      case NETMSGTYPE_SetChannelDirect: { msg = new SetChannelDirectMsgEx(); } break;

      // nodes messages
      case NETMSGTYPE_GenericDebug: { msg = new GenericDebugMsgEx(); } break;
      case NETMSGTYPE_GetClientStats: { msg = new GetClientStatsMsgEx(); } break;
      case NETMSGTYPE_GetNodeCapacityPools: { msg = new GetNodeCapacityPoolsMsgEx(); } break;
      case NETMSGTYPE_GetNodeCapacityPoolsResp: { msg = new GetNodeCapacityPoolsRespMsg(); } break;
      case NETMSGTYPE_GetNodes: { msg = new GetNodesMsgEx(); } break;
      case NETMSGTYPE_GetNodesResp: { msg = new GetNodesRespMsg(); } break;
      case NETMSGTYPE_GetTargetMappings: { msg = new GetTargetMappingsMsgEx(); } break;
      case NETMSGTYPE_GetTargetMappingsResp: { msg = new GetTargetMappingsRespMsg(); } break;
      case NETMSGTYPE_HeartbeatRequest: { msg = new HeartbeatRequestMsgEx(); } break;
      case NETMSGTYPE_Heartbeat: { msg = new HeartbeatMsgEx(); } break;
      case NETMSGTYPE_MapTargets: { msg = new MapTargetsMsgEx(); } break;
      case NETMSGTYPE_RegisterNodeResp: { msg = new RegisterNodeRespMsg(); } break;
      case NETMSGTYPE_RemoveNode: { msg = new RemoveNodeMsgEx(); } break;
      case NETMSGTYPE_RemoveNodeResp: { msg = new RemoveNodeRespMsg(); } break;
      case NETMSGTYPE_RefresherControl: { msg = new RefresherControlMsgEx(); } break;
      case NETMSGTYPE_RefreshCapacityPools: { msg = new RefreshCapacityPoolsMsgEx(); } break;

      // storage messages
      case NETMSGTYPE_FindOwner: { msg = new FindOwnerMsgEx(); } break;
      case NETMSGTYPE_FindOwnerResp: { msg = new FindOwnerRespMsg(); } break;
      case NETMSGTYPE_GetHighResStats: { msg = new GetHighResStatsMsgEx(); } break;
      case NETMSGTYPE_GetChunkFileAttribsResp: { msg = new GetChunkFileAttribsRespMsg(); } break;
      case NETMSGTYPE_ListDirFromOffset: { msg = new ListDirFromOffsetMsgEx(); } break;
      case NETMSGTYPE_ListDirFromOffsetResp: { msg = new ListDirFromOffsetRespMsg(); } break;
      case NETMSGTYPE_MkDir: { msg = new MkDirMsgEx(); } break;
      case NETMSGTYPE_MkDirResp: { msg = new MkDirRespMsg(); } break;
      case NETMSGTYPE_MkFile: { msg = new MkFileMsgEx(); } break;
      case NETMSGTYPE_MkFileResp: { msg = new MkFileRespMsg(); } break;
      case NETMSGTYPE_MkFileWithPattern: { msg = new MkFileWithPatternMsgEx(); } break;
      case NETMSGTYPE_MkFileWithPatternResp: { msg = new MkFileWithPatternRespMsg(); } break;
      case NETMSGTYPE_MkLocalDir: { msg = new MkLocalDirMsgEx(); } break;
      case NETMSGTYPE_MkLocalDirResp: { msg = new MkLocalDirRespMsg(); } break;
      case NETMSGTYPE_MkLocalFileResp: { msg = new MkLocalFileRespMsg(); } break;
      case NETMSGTYPE_MovingDirInsert: { msg = new MovingDirInsertMsgEx(); } break;
      case NETMSGTYPE_MovingDirInsertResp: { msg = new MovingDirInsertRespMsg(); } break;
      case NETMSGTYPE_MovingFileInsert: { msg = new MovingFileInsertMsgEx(); } break;
      case NETMSGTYPE_MovingFileInsertResp: { msg = new MovingFileInsertRespMsg(); } break;
      case NETMSGTYPE_RefreshEntryInfo: { msg = new RefreshEntryInfoMsgEx(); } break;
      case NETMSGTYPE_Rename: { msg = new RenameV2MsgEx(); } break;
      case NETMSGTYPE_RenameResp: { msg = new RenameRespMsg(); } break;
      case NETMSGTYPE_RmDir: { msg = new RmDirMsgEx(); } break;
      case NETMSGTYPE_RmDirResp: { msg = new RmDirRespMsg(); } break;
      case NETMSGTYPE_RmDirEntry: { msg = new RmDirEntryMsgEx(); } break;
      case NETMSGTYPE_RmLocalDir: { msg = new RmLocalDirMsgEx(); } break;
      case NETMSGTYPE_RmLocalDirResp: { msg = new RmLocalDirRespMsg(); } break;
      case NETMSGTYPE_UnlinkFile: { msg = new UnlinkFileMsgEx(); } break;
      case NETMSGTYPE_UnlinkFileResp: { msg = new UnlinkFileRespMsg(); } break;
      case NETMSGTYPE_UnlinkLocalFileResp: { msg = new UnlinkLocalFileRespMsg(); } break;
      case NETMSGTYPE_GetEntryInfo: { msg = new GetEntryInfoMsgEx(); } break;
      case NETMSGTYPE_LookupIntent: { msg = new LookupIntentMsgEx(); } break;
      case NETMSGTYPE_SetAttr: { msg = new SetAttrMsgEx(); } break;
      case NETMSGTYPE_SetAttrResp: { msg = new SetAttrRespMsg(); } break;
      case NETMSGTYPE_SetLocalAttrResp: { msg = new SetLocalAttrRespMsg(); } break;
      case NETMSGTYPE_SetDirPattern: { msg = new SetDirPatternMsgEx(); } break;
      case NETMSGTYPE_Stat: { msg = new StatMsgEx(); } break;
      case NETMSGTYPE_StatResp: { msg = new StatRespMsg(); } break;
      case NETMSGTYPE_StatStoragePath: { msg = new StatStoragePathMsgEx(); } break;
      case NETMSGTYPE_StatStoragePathResp: { msg = new StatStoragePathRespMsg(); } break;
      case NETMSGTYPE_TruncFile: { msg = new TruncFileMsgEx(); } break;
      case NETMSGTYPE_TruncFileResp: { msg = new TruncFileRespMsg(); } break;
      case NETMSGTYPE_TruncLocalFileResp: { msg = new TruncLocalFileRespMsg(); } break;
      case NETMSGTYPE_FindEntryname: { msg = new FindEntrynameMsgEx(); } break;
      case NETMSGTYPE_FindLinkOwner: { msg = new FindLinkOwnerMsgEx(); } break;

      // session messages
      case NETMSGTYPE_OpenFile: { msg = new OpenFileMsgEx(); } break;
      case NETMSGTYPE_OpenFileResp: { msg = new OpenFileRespMsg(); } break;
      case NETMSGTYPE_CloseFile: { msg = new CloseFileMsgEx(); } break;
      case NETMSGTYPE_CloseFileResp: { msg = new CloseFileRespMsg(); } break;
      case NETMSGTYPE_CloseChunkFileResp: { msg = new CloseChunkFileRespMsg(); } break;
      case NETMSGTYPE_WriteLocalFileResp: { msg = new WriteLocalFileRespMsg(); } break;
      case NETMSGTYPE_FSyncLocalFileResp: { msg = new FSyncLocalFileRespMsg(); } break;
      //case NETMSGTYPE_AcquireAppendLock: { msg = new AcquireAppendLockMsgEx(); } break;
      //case NETMSGTYPE_AcquireAppendLockResp: { msg = new AcquireAppendLockRespMsg(); } break;
      //case NETMSGTYPE_ReleaseAppendLock: { msg = new ReleaseAppendLockMsgEx(); } break;
      //case NETMSGTYPE_ReleaseAppendLockResp: { msg = new ReleaseAppendLockRespMsg(); } break;
      case NETMSGTYPE_FLockEntry: { msg = new FLockEntryMsgEx(); } break;
      case NETMSGTYPE_FLockRange: { msg = new FLockRangeMsgEx(); } break;

      //admon messages
      case NETMSGTYPE_RequestMetaData: { msg = new RequestMetaDataMsgEx(); } break;
      case NETMSGTYPE_GetNodeInfo: { msg = new GetNodeInfoMsgEx(); } break;

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
      case NETMSGTYPE_RemoveInode: { msg = new RemoveInodeMsgEx(); } break;
      case NETMSGTYPE_RemoveInodeResp: { msg = new RemoveInodeRespMsg(); } break;
      case NETMSGTYPE_ChangeStripeTarget: { msg = new ChangeStripeTargetMsgEx(); } break;
      case NETMSGTYPE_RecreateFsIDs: { msg = new RecreateFsIDsMsgEx(); } break;
      case NETMSGTYPE_RecreateDentries: { msg = new RecreateDentriesMsgEx(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return msg;
}

