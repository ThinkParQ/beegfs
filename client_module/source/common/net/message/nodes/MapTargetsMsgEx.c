#include <app/App.h>
#include <common/nodes/Node.h>
#include <common/toolkit/SocketTk.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/toolkit/ListTk.h>
#include <nodes/NodeStoreEx.h>
#include <app/config/Config.h>
#include "MapTargetsMsgEx.h"


const struct NetMessageOps MapTargetsMsgEx_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = MapTargetsMsgEx_deserializePayload,
   .processIncoming = __MapTargetsMsgEx_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool MapTargetsMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   MapTargetsMsgEx* thisCast = (MapTargetsMsgEx*)this;

   // targets
   if(!Serialization_deserializeTargetPoolPairListPreprocess(ctx, &thisCast->rawTargetsList) )
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
   UInt16List targetIDs;
   UInt16ListIter iter;
   NumNodeID nodeID = MapTargetsMsgEx_getNodeID(thisCast);


   peer = fromAddr ?
      SocketTk_ipaddrToStr(&fromAddr->addr) : StringTk_strDup(Socket_getPeername(sock) );
   LOG_DEBUG_FORMATTED(log, 4, logContext, "Received a MapTargetsMsg from: %s", peer);
   kfree(peer);

   IGNORE_UNUSED_VARIABLE(log);
   IGNORE_UNUSED_VARIABLE(logContext);


   UInt16List_init(&targetIDs);

   MapTargetsMsgEx_parseTargetIDs(thisCast, &targetIDs);

   for(UInt16ListIter_init(&iter, &targetIDs);
       !UInt16ListIter_end(&iter);
       UInt16ListIter_next(&iter) )
   {
      uint16_t targetID = UInt16ListIter_value(&iter);

      bool wasNewTarget = TargetMapper_mapTarget(targetMapper, targetID, nodeID);
      if(wasNewTarget)
      {
         LOG_DEBUG_FORMATTED(log, Log_WARNING, logContext, "Mapping target %hu => node %hu",
            targetID, nodeID);
      }
   }

   // send ack
   MsgHelperAck_respondToAckRequest(app, MapTargetsMsgEx_getAckID(thisCast), fromAddr, sock,
      respBuf, bufLen);

   // clean-up
   UInt16List_uninit(&targetIDs);

   return true;
}

