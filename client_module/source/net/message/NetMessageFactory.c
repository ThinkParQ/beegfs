// control messages
#include <common/net/message/control/AckMsgEx.h>
#include <common/net/message/control/GenericResponseMsg.h>
// helperd messages
#include <common/net/message/helperd/GetHostByNameRespMsg.h>
#include <common/net/message/helperd/LogRespMsg.h>
// nodes messages
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/nodes/GetStatesAndBuddyGroupsRespMsg.h>
#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsgEx.h>
#include <common/net/message/nodes/HeartbeatMsgEx.h>
#include <common/net/message/nodes/MapTargetsMsgEx.h>
#include <common/net/message/nodes/RefreshTargetStatesMsgEx.h>
#include <common/net/message/nodes/RegisterNodeRespMsg.h>
#include <common/net/message/nodes/RemoveNodeMsgEx.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/nodes/SetMirrorBuddyGroupMsgEx.h>
// storage messages
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include <common/net/message/storage/creating/HardlinkRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/net/message/storage/attribs/ListXAttrRespMsg.h>
#include <common/net/message/storage/attribs/GetXAttrRespMsg.h>
#include <common/net/message/storage/attribs/RemoveXAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetXAttrRespMsg.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/net/message/storage/TruncFileRespMsg.h>
// session messages
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#ifdef BEEGFS_NVFS
#include <common/net/message/session/rw/WriteLocalFileRDMARespMsg.h>
#endif
#include <common/net/message/session/BumpFileVersionResp.h>
#include <common/net/message/session/FSyncLocalFileRespMsg.h>
#include <common/net/message/session/GetFileVersionRespMsg.h>
#include <common/net/message/session/locking/FLockAppendRespMsg.h>
#include <common/net/message/session/locking/FLockEntryRespMsg.h>
#include <common/net/message/session/locking/FLockRangeRespMsg.h>
#include <common/net/message/session/locking/LockGrantedMsgEx.h>

#include <common/net/message/SimpleMsg.h>
#include "NetMessageFactory.h"


static bool __NetMessageFactory_deserializeRaw(App* app, DeserializeCtx* ctx,
   NetMessageHeader* header, NetMessage* outMsg)
{
   bool checkCompatRes;
   bool deserPayloadRes;

   outMsg->msgHeader = *header;

   checkCompatRes = NetMessage_checkHeaderFeatureFlagsCompat(outMsg);
   if(unlikely(!checkCompatRes) )
   { // incompatible feature flag was set => log error with msg type
      printk_fhgfs(KERN_NOTICE,
         "Received a message with incompatible feature flags. Message type: %hu; Flags (hex): %X",
         header->msgType, NetMessage_getMsgHeaderFeatureFlags(outMsg) );

      _NetMessage_setMsgType(outMsg, NETMSGTYPE_Invalid);
      return false;
   }

   // deserialize message payload

   deserPayloadRes = outMsg->ops->deserializePayload(outMsg, ctx);
   if(unlikely(!deserPayloadRes) )
   {
      printk_fhgfs_debug(KERN_NOTICE, "Failed to decode message. Message type: %hu",
         header->msgType);

      _NetMessage_setMsgType(outMsg, NETMSGTYPE_Invalid);
      return false;
   }

   return true;
}

/**
 * The standard way to create message objects from serialized message buffers.
 *
 * @return NetMessage which must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
NetMessage* NetMessageFactory_createFromBuf(App* app, char* recvBuf, size_t bufLen)
{
   NetMessageHeader header;
   NetMessage* msg;
   DeserializeCtx ctx = {
      .data = recvBuf,
      .length = bufLen,
   };

   // decode the message header
   __NetMessage_deserializeHeader(&ctx, &header);

   // create the message object for the given message type
   msg = NetMessageFactory_createFromMsgType(header.msgType);

   if(unlikely(NetMessage_getMsgType(msg) == NETMSGTYPE_Invalid) )
   {
      printk_fhgfs_debug(KERN_NOTICE,
         "Received an invalid or unhandled message. "
         "Message type (from raw header): %hu", header.msgType);

      return msg;
   }

   __NetMessageFactory_deserializeRaw(app, &ctx, &header, msg);
   return msg;
}

/**
 * Special deserializer for pre-alloc'ed message objects.
 *
 * @param outMsg prealloc'ed msg of the expected type; must be initialized with the corresponding
 * deserialization-initializer; must later be uninited/destructed by the caller, no matter whether
 * this succeeds or not
 * @param expectedMsgType the type of the pre-alloc'ed message object
 * @return false on error (e.g. if real msgType does not match expectedMsgType)
 */
bool NetMessageFactory_deserializeFromBuf(App* app, char* recvBuf, size_t bufLen,
   NetMessage* outMsg, unsigned short expectedMsgType)
{
   NetMessageHeader header;
   DeserializeCtx ctx = {
      .data = recvBuf,
      .length = bufLen,
   };

   // decode the message header
   __NetMessage_deserializeHeader(&ctx, &header);

   // verify expected message type
   if(unlikely(header.msgType != expectedMsgType) )
      return false;

   return __NetMessageFactory_deserializeRaw(app, &ctx, &header, outMsg);
}


/**
 * @return NetMessage that must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
NetMessage* NetMessageFactory_createFromMsgType(unsigned short msgType)
{
#define HANDLE(ID, TYPE) case NETMSGTYPE_##ID: return NETMESSAGE_CONSTRUCT(TYPE)

   switch(msgType)
   {
      // control messages
      HANDLE(Ack, AckMsgEx);
      HANDLE(GenericResponse, GenericResponseMsg);
      // helperd messages
      HANDLE(GetHostByNameResp, GetHostByNameRespMsg);
      HANDLE(LogResp, LogRespMsg);
      // nodes messages
      HANDLE(GetNodesResp, GetNodesRespMsg);
      HANDLE(GetStatesAndBuddyGroupsResp, GetStatesAndBuddyGroupsRespMsg);
      HANDLE(GetTargetMappingsResp, GetTargetMappingsRespMsg);
      HANDLE(HeartbeatRequest, HeartbeatRequestMsgEx);
      HANDLE(Heartbeat, HeartbeatMsgEx);
      HANDLE(MapTargets, MapTargetsMsgEx);
      HANDLE(RefreshTargetStates, RefreshTargetStatesMsgEx);
      HANDLE(RegisterNodeResp, RegisterNodeRespMsg);
      HANDLE(RemoveNode, RemoveNodeMsgEx);
      HANDLE(RemoveNodeResp, RemoveNodeRespMsg);
      HANDLE(SetMirrorBuddyGroup, SetMirrorBuddyGroupMsgEx);
      // storage messages
      HANDLE(LookupIntentResp, LookupIntentRespMsg);
      HANDLE(MkDirResp, MkDirRespMsg);
      HANDLE(RmDirResp, RmDirRespMsg);
      HANDLE(MkFileResp, MkFileRespMsg);
      HANDLE(RefreshEntryInfoResp, RefreshEntryInfoRespMsg);
      HANDLE(RenameResp, RenameRespMsg);
      HANDLE(HardlinkResp, HardlinkRespMsg);;
      HANDLE(UnlinkFileResp, UnlinkFileRespMsg);
      HANDLE(ListDirFromOffsetResp, ListDirFromOffsetRespMsg);
      HANDLE(SetAttrResp, SetAttrRespMsg);
      HANDLE(StatResp, StatRespMsg);
      HANDLE(StatStoragePathResp, StatStoragePathRespMsg);
      HANDLE(TruncFileResp, TruncFileRespMsg);
      HANDLE(ListXAttrResp, ListXAttrRespMsg);
      HANDLE(GetXAttrResp, GetXAttrRespMsg);
      HANDLE(RemoveXAttrResp, RemoveXAttrRespMsg);
      HANDLE(SetXAttrResp, SetXAttrRespMsg);
      // session messages
      HANDLE(OpenFileResp, OpenFileRespMsg);
      HANDLE(CloseFileResp, CloseFileRespMsg);
      HANDLE(WriteLocalFileResp, WriteLocalFileRespMsg);
#ifdef BEEGFS_NVFS
      HANDLE(WriteLocalFileRDMAResp, WriteLocalFileRDMARespMsg);
#endif
      HANDLE(FSyncLocalFileResp, FSyncLocalFileRespMsg);
      HANDLE(FLockAppendResp, FLockAppendRespMsg);
      HANDLE(FLockEntryResp, FLockEntryRespMsg);
      HANDLE(FLockRangeResp, FLockRangeRespMsg);
      HANDLE(LockGranted, LockGrantedMsgEx);
      HANDLE(BumpFileVersionResp, BumpFileVersionRespMsg);
      HANDLE(GetFileVersionResp, GetFileVersionRespMsg);

      case NETMSGTYPE_AckNotifyResp: {
         SimpleMsg* msg = os_kmalloc(sizeof(*msg));
         SimpleMsg_init(msg, msgType);
         return &msg->netMessage;
      }

      default:
      {
         SimpleMsg* msg = os_kmalloc(sizeof(*msg));
         SimpleMsg_init(msg, NETMSGTYPE_Invalid);
         return (NetMessage*)msg;
      }
   }
}


