#include <app/App.h>
#include <common/nodes/Node.h>
#include <common/toolkit/SocketTk.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/toolkit/ListTk.h>
#include <nodes/NodeStoreEx.h>
#include <app/config/Config.h>
#include "MapTargetsMsgEx.h"


static void MapTargetsMsgEx_release(NetMessage* msg)
{
   MapTargetsMsgEx* this = container_of(msg, struct MapTargetsMsgEx, netMessage);

   BEEGFS_KFREE_LIST(&this->poolMappings, struct TargetPoolMapping, _list);
}

const struct NetMessageOps MapTargetsMsgEx_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = MapTargetsMsgEx_deserializePayload,
   .processIncoming = __MapTargetsMsgEx_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .release = MapTargetsMsgEx_release,
};

bool MapTargetsMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   MapTargetsMsgEx* thisCast = (MapTargetsMsgEx*)this;

   // targets
   if (!TargetPoolMappingList_deserialize(ctx, &thisCast->poolMappings))
      return false;

   // nodeID
   if(!NumNodeID_deserialize(ctx, &thisCast->nodeID) )
      return false;

   // ackID
   if(!Serialization_deserializeStr(ctx, &thisCast->ackIDLen, &thisCast->ackID) )
      return false;

   return true;
}

bool __MapTargetsMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "MapTargetsMsg incoming";

   MapTargetsMsgEx* thisCast = (MapTargetsMsgEx*)this;

   const char* peer;

   TargetMapper* targetMapper = App_getTargetMapper(app);
   NumNodeID nodeID = MapTargetsMsgEx_getNodeID(thisCast);
   struct TargetPoolMapping* mapping;


   peer = fromAddr ?
      SocketTk_ipaddrToStr(&fromAddr->addr) : StringTk_strDup(Socket_getPeername(sock) );
   LOG_DEBUG_FORMATTED(log, 4, logContext, "Received a MapTargetsMsg from: %s", peer);
   kfree(peer);

   IGNORE_UNUSED_VARIABLE(log);
   IGNORE_UNUSED_VARIABLE(logContext);

   list_for_each_entry(mapping, &thisCast->poolMappings, _list)
   {
      bool wasNewTarget = TargetMapper_mapTarget(targetMapper, mapping->targetID, nodeID);
      if(wasNewTarget)
      {
         LOG_DEBUG_FORMATTED(log, Log_WARNING, logContext, "Mapping target %hu => node %hu",
            mapping->targetID, nodeID.value);
      }
   }

   // send ack
   MsgHelperAck_respondToAckRequest(app, MapTargetsMsgEx_getAckID(thisCast), fromAddr, sock,
      respBuf, bufLen);

   return true;
}

