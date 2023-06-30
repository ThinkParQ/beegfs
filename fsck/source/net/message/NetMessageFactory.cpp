// control messages
#include <common/net/message/control/GenericResponseMsg.h>

// fsck messages
#include <common/net/message/fsck/AdjustChunkPermissionsRespMsg.h>
#include <common/net/message/fsck/CreateDefDirInodesRespMsg.h>
#include <common/net/message/fsck/CreateEmptyContDirsRespMsg.h>
#include <common/net/message/fsck/DeleteChunksRespMsg.h>
#include <common/net/message/fsck/DeleteDirEntriesRespMsg.h>
#include <common/net/message/fsck/FetchFsckChunkListRespMsg.h>
#include <common/net/message/fsck/FsckSetEventLoggingRespMsg.h>
#include <common/net/message/fsck/FixInodeOwnersInDentryRespMsg.h>
#include <common/net/message/fsck/FixInodeOwnersRespMsg.h>
#include <common/net/message/fsck/FsckSetEventLoggingRespMsg.h>
#include <common/net/message/fsck/LinkToLostAndFoundRespMsg.h>
#include <common/net/message/fsck/MoveChunkFileRespMsg.h>
#include <common/net/message/fsck/RecreateDentriesRespMsg.h>
#include <common/net/message/fsck/RecreateFsIDsRespMsg.h>
#include <common/net/message/fsck/RemoveInodesRespMsg.h>
#include <common/net/message/fsck/RetrieveDirEntriesRespMsg.h>
#include <common/net/message/fsck/RetrieveFsIDsRespMsg.h>
#include <common/net/message/fsck/RetrieveInodesRespMsg.h>
#include <common/net/message/fsck/UpdateDirAttribsRespMsg.h>
#include <common/net/message/fsck/UpdateFileAttribsRespMsg.h>
#include <net/message/fsck/FsckModificationEventMsgEx.h>
#include <common/net/message/fsck/CheckAndRepairDupInodeRespMsg.h>

// nodes messages
#include <common/net/message/nodes/GetMirrorBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <net/message/nodes/HeartbeatMsgEx.h>

// storage messages
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/net/message/storage/creating/RmDirEntryRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>

// general includes
#include <common/net/message/SimpleMsg.h>
#include <net/message/testing/DummyMsgEx.h>

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
      case NETMSGTYPE_GenericResponse: { msg = new GenericResponseMsg(); } break;

      // nodes messages
      case NETMSGTYPE_GetNodesResp: { msg = new GetNodesRespMsg(); } break;
      case NETMSGTYPE_Heartbeat: { msg = new HeartbeatMsgEx(); } break;
      case NETMSGTYPE_GetMirrorBuddyGroupsResp: { msg = new GetMirrorBuddyGroupsRespMsg(); } break;
      case NETMSGTYPE_GetTargetMappingsResp: { msg = new GetTargetMappingsRespMsg(); } break;
      case NETMSGTYPE_GetTargetStatesResp: { msg = new GetTargetStatesRespMsg(); } break;
      case NETMSGTYPE_SetTargetConsistencyStatesResp: { msg = new SetTargetConsistencyStatesRespMsg(); } break;

      // storage messages
      case NETMSGTYPE_FindOwnerResp: { msg = new FindOwnerRespMsg(); } break;
      case NETMSGTYPE_ListDirFromOffsetResp: { msg = new ListDirFromOffsetRespMsg(); } break;
      case NETMSGTYPE_RmDirEntryResp: { msg = new RmDirEntryRespMsg(); } break;
      case NETMSGTYPE_GetEntryInfoResp: { msg = new GetEntryInfoRespMsg(); } break;
      case NETMSGTYPE_StatResp: { msg = new StatRespMsg(); } break;
      case NETMSGTYPE_StatStoragePathResp: { msg = new StatStoragePathRespMsg(); } break;
      case NETMSGTYPE_MkDirResp: { msg = new MkDirRespMsg(); } break;
      case NETMSGTYPE_SetLocalAttrResp: { msg = new SetLocalAttrRespMsg(); } break;
      case NETMSGTYPE_UnlinkFileResp: { msg = new UnlinkFileRespMsg(); } break;
      case NETMSGTYPE_MoveFileInodeResp: {msg = new MoveFileInodeRespMsg(); } break;

      //fsck
      case NETMSGTYPE_RetrieveDirEntriesResp: { msg = new RetrieveDirEntriesRespMsg(); } break;
      case NETMSGTYPE_RetrieveInodesResp: { msg = new RetrieveInodesRespMsg(); } break;
      case NETMSGTYPE_RetrieveFsIDsResp: { msg = new RetrieveFsIDsRespMsg(); } break;
      case NETMSGTYPE_DeleteDirEntriesResp: { msg = new DeleteDirEntriesRespMsg(); } break;
      case NETMSGTYPE_CreateDefDirInodesResp: { msg = new CreateDefDirInodesRespMsg(); } break;
      case NETMSGTYPE_FixInodeOwnersResp: { msg = new FixInodeOwnersRespMsg(); } break;
      case NETMSGTYPE_FixInodeOwnersInDentryResp: { msg = new FixInodeOwnersInDentryRespMsg(); } break;
      case NETMSGTYPE_LinkToLostAndFoundResp : { msg = new LinkToLostAndFoundRespMsg(); } break;
      case NETMSGTYPE_DeleteChunksResp: { msg = new DeleteChunksRespMsg(); } break;
      case NETMSGTYPE_CreateEmptyContDirsResp: { msg = new CreateEmptyContDirsRespMsg(); } break;
      case NETMSGTYPE_UpdateFileAttribsResp: { msg = new UpdateFileAttribsRespMsg(); } break;
      case NETMSGTYPE_UpdateDirAttribsResp: { msg = new UpdateDirAttribsRespMsg(); } break;
      case NETMSGTYPE_RecreateFsIDsResp: { msg = new RecreateFsIDsRespMsg(); } break;
      case NETMSGTYPE_RecreateDentriesResp: { msg = new RecreateDentriesRespMsg(); } break;
      case NETMSGTYPE_FsckSetEventLoggingResp: { msg = new FsckSetEventLoggingRespMsg(); } break;
      case NETMSGTYPE_FsckModificationEvent: { msg = new FsckModificationEventMsgEx(); } break;
      case NETMSGTYPE_AdjustChunkPermissionsResp: { msg = new AdjustChunkPermissionsRespMsg(); } break;
      case NETMSGTYPE_FetchFsckChunkListResp: { msg = new FetchFsckChunkListRespMsg(); } break;
      case NETMSGTYPE_MoveChunkFileResp: { msg = new MoveChunkFileRespMsg(); } break;
      case NETMSGTYPE_RemoveInodesResp: { msg = new RemoveInodesRespMsg(); } break;
      case NETMSGTYPE_CheckAndRepairDupInodeResp: { msg = new CheckAndRepairDupInodeRespMsg(); } break;

      //testing
      case NETMSGTYPE_Dummy: { msg = new DummyMsgEx(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return std::unique_ptr<NetMessage>(msg);
}

