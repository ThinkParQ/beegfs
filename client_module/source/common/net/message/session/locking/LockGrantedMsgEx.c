#include <app/App.h>
#include <components/AckManager.h>
#include <common/toolkit/ackstore/AcknowledgmentStore.h>
#include <common/toolkit/SocketTk.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include "LockGrantedMsgEx.h"

const struct NetMessageOps LockGrantedMsgEx_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = LockGrantedMsgEx_deserializePayload,
   .processIncoming = __LockGrantedMsgEx_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool LockGrantedMsgEx_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   LockGrantedMsgEx* thisCast = (LockGrantedMsgEx*)this;

   // lockAckID
   if(!Serialization_deserializeStrAlign4(ctx,
      &thisCast->lockAckIDLen, &thisCast->lockAckID) )
      return false;

   // ackID
   if(!Serialization_deserializeStrAlign4(ctx, &thisCast->ackIDLen, &thisCast->ackID) )
      return false;

   // granterNodeID
   if(!NumNodeID_deserialize(ctx, &thisCast->granterNodeID) )
      return false;

   return true;
}

bool __LockGrantedMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   const char* logContext = "LockGranted incoming";
   Logger* log = App_getLogger(app);
   AcknowledgmentStore* ackStore = App_getAckStore(app);
   AckManager* ackManager = App_getAckManager(app);

   LockGrantedMsgEx* thisCast = (LockGrantedMsgEx*)this;
   bool gotLockWaiter;

   #ifdef LOG_DEBUG_MESSAGES
      const char* peer;

      peer = fromAddr ?
         SocketTk_ipaddrToStr(fromAddr->addr) : StringTk_strDup(Socket_getPeername(sock) );
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "Received a AckMsg from: %s", peer);

      kfree(peer);
   #endif // LOG_DEBUG_MESSAGES

   IGNORE_UNUSED_VARIABLE(log);
   IGNORE_UNUSED_VARIABLE(logContext);

   gotLockWaiter = AcknowledgmentStore_receivedAck(
      ackStore, LockGrantedMsgEx_getLockAckID(thisCast) );

   /* note: other than for standard acks, we send a ack repsonse to lock-grants only if someone was
      still registered to wait for the lock-grant. (we do this to make our handling of interrupts
      during lock waits easier, because we don't need lock canceling now.) */

   if(gotLockWaiter)
   { // lock waiter registered => send ack
      const char* ackID = LockGrantedMsgEx_getAckID(thisCast);
      NumNodeID granterNodeID = LockGrantedMsgEx_getGranterNodeID(thisCast);

      MsgHelperAck_respondToAckRequest(app, LockGrantedMsgEx_getAckID(thisCast), fromAddr, sock,
         respBuf, bufLen);

      AckManager_addAckToQueue(ackManager, granterNodeID, ackID);
   }

   return true;
}

