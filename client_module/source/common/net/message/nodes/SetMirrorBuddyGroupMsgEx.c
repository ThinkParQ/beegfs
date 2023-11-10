#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <app/App.h>

#include "SetMirrorBuddyGroupMsgEx.h"

const struct NetMessageOps SetMirrorBuddyGroupMsgEx_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = SetMirrorBuddyGroupMsgEx_deserializePayload,
   .processIncoming = __SetMirrorBuddyGroupMsgEx_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool SetMirrorBuddyGroupMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   SetMirrorBuddyGroupMsgEx* thisCast = (SetMirrorBuddyGroupMsgEx*)this;

   // nodeType
   if (!Serialization_deserializeInt(ctx, &thisCast->nodeType) )
      return false;

   // primaryTargetID
   if (!Serialization_deserializeUShort(ctx, &thisCast->primaryTargetID) )
      return false;

   // secondaryTargetID
   if (!Serialization_deserializeUShort(ctx, &thisCast->secondaryTargetID) )
      return false;

   // buddyGroupID
   if (!Serialization_deserializeUShort(ctx, &thisCast->buddyGroupID) )
      return false;

   // allowUpdate
   if (!Serialization_deserializeBool(ctx, &thisCast->allowUpdate) )
      return false;

   // ackID
   if (!Serialization_deserializeStr(ctx, &thisCast->ackIDLen, &thisCast->ackID) )
      return false;

   return true;
}

bool __SetMirrorBuddyGroupMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   Logger* log = App_getLogger(app);
   MirrorBuddyGroupMapper* mirrorBuddyGroupMapper;
   TargetMapper* targetMapper = NULL;
   const char* logContext = "SetMirrorBuddyGroupMsg Incoming";
   FhgfsOpsErr addGroupResult;

   SetMirrorBuddyGroupMsgEx* thisCast = (SetMirrorBuddyGroupMsgEx*)this;

   const char* peer;

   uint16_t primaryTargetID = SetMirrorBuddyGroupMsgEx_getPrimaryTargetID(thisCast);
   uint16_t secondaryTargetID = SetMirrorBuddyGroupMsgEx_getSecondaryTargetID(thisCast);
   uint16_t buddyGroupID = SetMirrorBuddyGroupMsgEx_getBuddyGroupID(thisCast);
   bool allowUpdate = SetMirrorBuddyGroupMsgEx_getAllowUpdate(thisCast);

   NodeType nodeType = SetMirrorBuddyGroupMsgEx_getNodeType(thisCast);

   switch (nodeType)
   {
      case NODETYPE_Storage:
         mirrorBuddyGroupMapper = App_getStorageBuddyGroupMapper(app);
         targetMapper = App_getTargetMapper(app);
         break;

      case NODETYPE_Meta:
         mirrorBuddyGroupMapper = App_getMetaBuddyGroupMapper(app);
         break;

      default:
         Logger_logErr(log, logContext, "Node type mismatch");
         return false;
   }

   addGroupResult = MirrorBuddyGroupMapper_addGroup(mirrorBuddyGroupMapper, app->cfg, targetMapper,
      buddyGroupID, primaryTargetID, secondaryTargetID, allowUpdate);

   if (addGroupResult == FhgfsOpsErr_SUCCESS)
      Logger_logFormatted(log, Log_NOTICE, logContext, "Added mirror group: Node type: %s; "
            "Group ID: %hu; Primary target ID: %hu; Secondary target ID: %hu.",
            Node_nodeTypeToStr(nodeType), buddyGroupID, primaryTargetID, secondaryTargetID);
   else
      Logger_logFormatted(log, Log_WARNING, logContext, "Error adding mirror buddy group: "
            "Node type: %s, Group ID: %hu; Primary target ID: %hu; Secondary target ID: %hu; "
            "Error: %s",
            Node_nodeTypeToStr(nodeType), buddyGroupID, primaryTargetID, secondaryTargetID,
            FhgfsOpsErr_toErrString(addGroupResult) );

   peer = fromAddr ?
      SocketTk_ipaddrToStr(fromAddr->addr) : StringTk_strDup(Socket_getPeername(sock) );

   // send Ack
   MsgHelperAck_respondToAckRequest(app, SetMirrorBuddyGroupMsgEx_getAckID(thisCast), fromAddr,
      sock, respBuf, bufLen);

   return true;
}
