#include <app/App.h>
#include <common/toolkit/SocketTk.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include <components/InternodeSyncer.h>

#include "RefreshTargetStatesMsgEx.h"


const struct NetMessageOps RefreshTargetStatesMsgEx_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = RefreshTargetStatesMsgEx_deserializePayload,
   .processIncoming = __RefreshTargetStatesMsgEx_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool RefreshTargetStatesMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   RefreshTargetStatesMsgEx* thisCast = (RefreshTargetStatesMsgEx*)this;

   // ackID
   if(!Serialization_deserializeStr(ctx, &thisCast->ackIDLen, &thisCast->ackID) )
      return false;

   return true;
}

bool __RefreshTargetStatesMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "RefreshTargetStatesMsg incoming";

   RefreshTargetStatesMsgEx* thisCast = (RefreshTargetStatesMsgEx*)this;

   const char* peer;

   InternodeSyncer* internodeSyncer = App_getInternodeSyncer(app);
   InternodeSyncer_setForceTargetStatesUpdate(internodeSyncer);

   peer = fromAddr ?
      SocketTk_ipaddrToStr(fromAddr->addr) : StringTk_strDup(Socket_getPeername(sock) );
   LOG_DEBUG_FORMATTED(log, 4, logContext, "Received a RefreshTargetStatesMsg from: %s", peer);
   kfree(peer);

   IGNORE_UNUSED_VARIABLE(log);
   IGNORE_UNUSED_VARIABLE(logContext);

   // send ack
   MsgHelperAck_respondToAckRequest(app, RefreshTargetStatesMsgEx_getAckID(thisCast), fromAddr,
      sock, respBuf, bufLen);

   return true;
}
