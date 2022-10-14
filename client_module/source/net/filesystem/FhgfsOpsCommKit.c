#include <app/App.h>
#include <common/net/message/session/FSyncLocalFileMsg.h>
#include <common/net/message/session/FSyncLocalFileRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/session/rw/ReadLocalFileV2Msg.h>
#include <common/net/message/storage/StatStoragePathMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/net/message/session/rw/WriteLocalFileMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/TargetStateStore.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <common/threading/Thread.h>
#include <fault-inject/fault-inject.h>
#include <linux/mempool.h>
#include "FhgfsOpsCommKit.h"


#define COMMKIT_RESET_SLEEP_MS        5000 /* how long to sleep if target state not good/offline */

static struct kmem_cache* headerBufferCache;
static mempool_t* headerBufferPool;

bool FhgfsOpsCommKit_initEmergencyPools()
{
   const char* cacheName = BEEGFS_MODULE_NAME_STR "-msgheaders";

   if(BEEGFS_COMMKIT_MSGBUF_SIZE > PAGE_SIZE)
   {
      WARN_ON(1);
      return false;
   }

#ifdef KERNEL_HAS_KMEMCACHE_DTOR
   headerBufferCache = kmem_cache_create(cacheName, BEEGFS_COMMKIT_MSGBUF_SIZE, 0,
      SLAB_MEM_SPREAD, NULL, NULL);
#else
   headerBufferCache = kmem_cache_create(cacheName, BEEGFS_COMMKIT_MSGBUF_SIZE, 0,
      SLAB_MEM_SPREAD, NULL);
#endif

   if(!headerBufferCache)
      return false;

   headerBufferPool = mempool_create_slab_pool(4, headerBufferCache);
   if(!headerBufferPool)
   {
      kmem_cache_destroy(headerBufferCache);
      return false;
   }

   return true;
}

void FhgfsOpsCommKit_releaseEmergencyPools()
{
   mempool_destroy(headerBufferPool);
   kmem_cache_destroy(headerBufferCache);
}

static void* allocHeaderBuffer(gfp_t gfp)
{
   return mempool_alloc(headerBufferPool, gfp);
}

static void freeHeaderBuffer(void* header)
{
   mempool_free(header, headerBufferPool);
}

static const struct CommKitContextOps readfileOps;
static const struct CommKitContextOps writefileOps;

struct ReadfileIterOps
{
   void (*nextIter)(CommKitContext*, ReadfileState*);
   void (*prepare)(CommKitContext*, ReadfileState*);
};

struct WritefileIterOps
{
   void (*nextIter)(CommKitContext*, WritefileState*);
   void (*prepare)(CommKitContext*, WritefileState*);
};

static void commkit_initTargetInfo(struct CommKitTargetInfo* info, uint16_t targetID)
{
   struct CommKitTargetInfo value = {
      .state = CommKitState_PREPARE,
      .targetID = targetID,
      .useBuddyMirrorSecond = false,
      .nodeResult = -FhgfsOpsErr_INTERNAL,
   };

   *info = value;
}

/**
 * Note: Initializes the expectedNodeResult attribute from the size argument
 */
void FhgfsOpsCommKit_initReadfileState(ReadfileState* state, loff_t offset, size_t size,
   uint16_t targetID)
{
   *state = (ReadfileState) {
      .offset = offset,
      .transmitted = 0,
      .toBeTransmitted = 0, // RECVHEADER will bump this in each HEADER-DATA loop iteration
      .totalReadSize = size,

      .firstWriteDoneForTarget = false,
      .receiveFileData = false,
      .expectedNodeResult = size,
   };

   commkit_initTargetInfo(&state->base, targetID);
}

/**
 * Note: Initializes the expectedNodeResult attribute from the size argument
 * Note: defaults to server-side mirroring enabled.
 */
void FhgfsOpsCommKit_initWritefileState(WritefileState* state, loff_t offset, size_t size,
   uint16_t targetID)
{
   *state = (WritefileState) {
      .offset = offset,
      .transmitted = 0,
      .toBeTransmitted = 0, // PREPARE sets this to totalWriteSize
      .totalWriteSize = size,

      .firstWriteDoneForTarget = false,
      .expectedNodeResult = size,
   };

   commkit_initTargetInfo(&state->base, targetID);
}

void FhgfsOpsCommKit_initFsyncState(struct FsyncContext* context, struct FsyncState* state,
   uint16_t targetID)
{
   commkit_initTargetInfo(&state->base, targetID);
   state->firstWriteDoneForTarget = false;

   INIT_LIST_HEAD(&state->base.targetInfoList);
   list_add_tail(&state->base.targetInfoList, &context->states);
}

void FhgfsOpsCommKit_initStatStorageState(struct list_head* states,
   struct StatStorageState* state, uint16_t targetID)
{
   commkit_initTargetInfo(&state->base, targetID);

   INIT_LIST_HEAD(&state->base.targetInfoList);
   list_add_tail(&state->base.targetInfoList, states);
}



static void __commkit_add_socket_pollstate(CommKitContext* context,
   struct CommKitTargetInfo* info, short pollEvents)
{
   PollState_addSocket(&context->pollState, info->socket, pollEvents);
   context->numPollSocks++;
}

static bool __commkit_prepare_io(CommKitContext* context, struct CommKitTargetInfo* info,
   int events)
{
   if (fatal_signal_pending(current) || !Node_getIsActive(info->node))
   {
      info->state = CommKitState_SOCKETINVALIDATE;
      return false;
   }

   // check for a poll() timeout or error (all states have to be cancelled in that case)
   if(unlikely(context->pollTimedOut) || BEEGFS_SHOULD_FAIL(commkit_polltimeout, 1) )
   {
      info->nodeResult = -FhgfsOpsErr_COMMUNICATION;
      info->state = CommKitState_SOCKETINVALIDATE;
      return false;
   }

   if(!(info->socket->poll.revents & events) )
   {
      __commkit_add_socket_pollstate(context, info, events);
      return false;
   }

   return true;
}


static bool __commkit_prepare_generic(CommKitContext* context, struct CommKitTargetInfo* info)
{
   TargetMapper* targetMapper = App_getTargetMapper(context->app);
   NodeStoreEx* storageNodes = App_getStorageNodes(context->app);

   FhgfsOpsErr resolveErr;
   NodeConnPool* connPool;
   bool allowWaitForConn = !context->numAcquiredConns; // don't wait if we got at least
      // one conn already (this is important to avoid a deadlock between racing commkit processes)

   info->socket = NULL;
   info->nodeResult = -FhgfsOpsErr_COMMUNICATION;
   info->selectedTargetID = info->targetID;

   info->headerBuffer = allocHeaderBuffer(allowWaitForConn ? GFP_NOFS : GFP_NOWAIT);
   if(!info->headerBuffer)
   {
      context->numBufferless += 1;
      return false;
   }

   // select the right targetID and get target state

   if(!context->ioInfo)
      info->selectedTargetID = info->targetID;
   else
   if(StripePattern_getPatternType(context->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = App_getStorageBuddyGroupMapper(context->app);

      info->selectedTargetID = info->useBuddyMirrorSecond ?
         MirrorBuddyGroupMapper_getSecondaryTargetID(mirrorBuddies, info->targetID) :
         MirrorBuddyGroupMapper_getPrimaryTargetID(mirrorBuddies, info->targetID);

      if(unlikely(!info->selectedTargetID) )
      { // invalid mirror group ID
         Logger_logErrFormatted(context->log, context->ops->logContext,
            "Invalid mirror buddy group ID: %hu", info->targetID);
         info->nodeResult = -FhgfsOpsErr_UNKNOWNTARGET;
         goto cleanup;
      }
   }

   // check target state

   {
      TargetStateStore* stateStore = App_getTargetStateStore(context->app);
      CombinedTargetState targetState;
      bool getStateRes = TargetStateStore_getState(stateStore, info->selectedTargetID,
         &targetState);

      if(unlikely( !getStateRes ||
          (targetState.reachabilityState == TargetReachabilityState_OFFLINE) ||
          ( context->ioInfo &&
            (StripePattern_getPatternType(context->ioInfo->pattern) ==
             STRIPEPATTERN_BuddyMirror) &&
            (targetState.consistencyState != TargetConsistencyState_GOOD) ) ) )
      { // unusable target state, retry details will be handled in retry handler
         int targetAction = CK_SKIP_TARGET;

         info->state = CommKitState_CLEANUP;

         if(context->ops->selectedTargetBad)
            targetAction = context->ops->selectedTargetBad(context, info, &targetState);

         if(targetAction == CK_SKIP_TARGET)
            goto error;
      }
   }

   // get the target-node reference
   info->node = NodeStoreEx_referenceNodeByTargetID(storageNodes,
      info->selectedTargetID, targetMapper, &resolveErr);
   if(unlikely(!info->node) )
   { // unable to resolve targetID
      info->nodeResult = -resolveErr;
      goto cleanup;
   }

   info->headerSize = context->ops->prepareHeader(context, info);
   if(info->headerSize == 0)
      goto cleanup;

   connPool = Node_getConnPool(info->node);

   // connect
   info->socket = NodeConnPool_acquireStreamSocketEx(connPool, allowWaitForConn);
   if(!info->socket)
   { // no conn available => error or didn't want to wait
      if(likely(!allowWaitForConn) )
      { // just didn't want to wait => keep stage and try again later
         Node_put(info->node);
         info->node = NULL;

         context->numUnconnectable++;
         goto error;
      }
      else
      { // connection error
         if(!context->connFailedLogged)
         { // no conn error logged yet
            if (fatal_signal_pending(current))
               Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
                  "Connect to server canceled by pending signal: %s",
                  Node_getNodeIDWithTypeStr(info->node) );
            else
               Logger_logFormatted(context->log, Log_WARNING, context->ops->logContext,
                  "Unable to connect to server: %s",
                  Node_getNodeIDWithTypeStr(info->node) );
         }

         context->connFailedLogged = true;
         goto cleanup;
      }
   }

   context->numAcquiredConns++;

   info->state = CommKitState_SENDHEADER;

   return true;

cleanup:
   info->state = CommKitState_CLEANUP;
   return false;

error:
   freeHeaderBuffer(info->headerBuffer);
   info->headerBuffer = NULL;
   return false;
}

static void __commkit_sendheader_generic(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   ssize_t sendRes;

   if(BEEGFS_SHOULD_FAIL(commkit_sendheader_timeout, 1) )
      sendRes = -ETIMEDOUT;
   else
      sendRes = Socket_send(info->socket, info->headerBuffer, info->headerSize, 0);

   if(unlikely(sendRes != info->headerSize) )
   {
      Logger_logFormatted(context->log, Log_WARNING, context->ops->logContext,
         "Failed to send message to %s: %s", Node_getNodeIDWithTypeStr(info->node),
         Socket_getPeername(info->socket) );

      info->state = CommKitState_SOCKETINVALIDATE;
      return;
   }

   info->state = CommKitState_SENDDATA;
   info->headerSize = 0;
}

static void __commkit_senddata_generic(CommKitContext* context, struct CommKitTargetInfo* info)
{
   int sendRes;

   if(!__commkit_prepare_io(context, info, POLLOUT) )
      return;

   sendRes = context->ops->sendData(context, info);

   if(unlikely(sendRes < 0) )
   {
      if(sendRes == -EFAULT)
      { // bad buffer address given
         Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
            "Bad buffer address");

         info->nodeResult = -FhgfsOpsErr_ADDRESSFAULT;
         info->state = CommKitState_SOCKETINVALIDATE;
         return;
      }

      Logger_logErrFormatted(context->log, context->ops->logContext,
         "Communication error in SENDDATA stage. Node: %s", Node_getNodeIDWithTypeStr(info->node) );
      if(context->ops->printSendDataDetails)
         context->ops->printSendDataDetails(context, info);

      info->state = CommKitState_SOCKETINVALIDATE;
      return;
   }

   if(sendRes == 0)
   { // all of the data has been sent => proceed to the next stage
      info->state = CommKitState_RECVHEADER;

      __commkit_add_socket_pollstate(context, info, POLLIN);
      return;
   }

   // there is still data to be sent => prepare pollout for the next round
   __commkit_add_socket_pollstate(context, info, POLLOUT);
}

static void __commkit_recvheader_generic(CommKitContext* context, struct CommKitTargetInfo* info)
{
   ssize_t recvRes = -EREMOTEIO;

   // check for incoming data
   if(!__commkit_prepare_io(context, info, POLLIN) )
      return;

   if(BEEGFS_SHOULD_FAIL(commkit_recvheader_timeout, 1) )
      recvRes = -ETIMEDOUT;
   else
   {
      size_t msgLength;

      if(info->headerSize < NETMSG_MIN_LENGTH)
      {
         recvRes = Socket_recvT(info->socket, info->headerBuffer + info->headerSize,
            NETMSG_MIN_LENGTH - info->headerSize, MSG_DONTWAIT, 0);
         if(recvRes <= 0)
         {
            Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
               "Failed to receive message header from: %s", Socket_getPeername(info->socket) );
            goto recv_err;
         }

         info->headerSize += recvRes;
      }

      msgLength = NetMessage_extractMsgLengthFromBuf(info->headerBuffer);

      if(msgLength > BEEGFS_COMMKIT_MSGBUF_SIZE)
      { // message too big to be accepted
         Logger_logFormatted(context->log, Log_WARNING, context->ops->logContext,
            "Received a message that is too large from: %s (bufLen: %u, msgLen: %zdd)",
            Socket_getPeername(info->socket), BEEGFS_COMMKIT_MSGBUF_SIZE, msgLength);

         info->state = -CommKitState_SOCKETINVALIDATE;
         info->nodeResult = -FhgfsOpsErr_COMMUNICATION;
         return;
      }

      if(info->headerSize < msgLength)
      {
         recvRes = Socket_recvT(info->socket, info->headerBuffer + info->headerSize,
            msgLength - info->headerSize, MSG_DONTWAIT, 0);
         if(recvRes <= 0)
         {
            Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
               "Failed to receive message body from: %s", Socket_getPeername(info->socket) );
            goto recv_err;
         }

         info->headerSize += recvRes;
      }

      if(info->headerSize < msgLength)
         return;
   }

recv_err:
   if(unlikely(recvRes <= 0) )
   { // receive failed
      // note: signal pending log msg will be printed in stage SOCKETEXCEPTION, so no need here
      if (!fatal_signal_pending(current))
         Logger_logFormatted(context->log, Log_WARNING, context->ops->logContext,
            "Receive failed from: %s @ %s", Node_getNodeIDWithTypeStr(info->node),
            Socket_getPeername(info->socket) );

      info->state = CommKitState_SOCKETINVALIDATE;
      return;
   }

   recvRes = context->ops->recvHeader(context, info);

   if(unlikely(recvRes < 0) )
      info->state = CommKitState_SOCKETINVALIDATE;
   else
      info->state = CommKitState_RECVDATA;
}

static void __commkit_recvdata_generic(CommKitContext* context, struct CommKitTargetInfo* info)
{
   int recvRes;

   if(!__commkit_prepare_io(context, info, POLLIN) )
      return;

   recvRes = context->ops->recvData(context, info);

   if(unlikely(recvRes < 0) )
   {
      if(recvRes == -EFAULT)
      { // bad buffer address given
         Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
            "Bad buffer address");

         info->nodeResult = -FhgfsOpsErr_ADDRESSFAULT;
         info->state = CommKitState_SOCKETINVALIDATE;
         return;
      }
      else
      if(recvRes == -ETIMEDOUT)
      { // timeout
         Logger_logErrFormatted(context->log, context->ops->logContext,
            "Communication timeout in RECVDATA stage. Node: %s",
            Node_getNodeIDWithTypeStr(info->node) );
      }
      else
      { // error
         Logger_logErrFormatted(context->log, context->ops->logContext,
            "Communication error in RECVDATA stage. Node: %s (recv result: %lld)",
            Node_getNodeIDWithTypeStr(info->node), (long long)recvRes);
      }

      info->state = CommKitState_SOCKETINVALIDATE;
      return;
   }

   if(recvRes == 0)
      info->state = CommKitState_CLEANUP;
   else
      __commkit_add_socket_pollstate(context, info, POLLIN);
}

static void __commkit_socketinvalidate_generic(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   if (fatal_signal_pending(current))
   { // interrupted by signal
      info->nodeResult = -FhgfsOpsErr_INTERRUPTED;
      Logger_logFormatted(context->log, Log_NOTICE, context->ops->logContext,
         "Communication interrupted by signal. Node: %s", Node_getNodeIDWithTypeStr(info->node) );
   }
   else
   if(!Node_getIsActive(info->node) )
   {
      info->nodeResult = -FhgfsOpsErr_UNKNOWNNODE;
      Logger_logErrFormatted(context->log, context->ops->logContext,
         "Communication with inactive node. Node: %s", Node_getNodeIDWithTypeStr(info->node) );
   }
   else if (info->nodeResult == -FhgfsOpsErr_ADDRESSFAULT)
   {
      // not a commkit error. release all resources and treat this CTI as done during cleanup.
   }
   else
   { // "normal" connection error
      info->nodeResult = -FhgfsOpsErr_COMMUNICATION;
      Logger_logErrFormatted(context->log, context->ops->logContext,
         "Communication error. Node: %s", Node_getNodeIDWithTypeStr(info->node) );

      if(context->ops->printSocketDetails)
         context->ops->printSocketDetails(context, info);
   }

   NodeConnPool_invalidateStreamSocket(Node_getConnPool(info->node), info->socket);
   context->numAcquiredConns--;
   info->socket = NULL;

   info->state = CommKitState_CLEANUP;
}

static void __commkit_cleanup_generic(CommKitContext* context, struct CommKitTargetInfo* info)
{
   if(likely(info->socket) )
   {
      NodeConnPool_releaseStreamSocket(Node_getConnPool(info->node), info->socket);
      context->numAcquiredConns--;
   }

   freeHeaderBuffer(info->headerBuffer);
   info->headerBuffer = NULL;

   if(likely(info->node) )
   {
      Node_put(info->node);
      info->node = NULL;
   }

   // prepare next stage

   if(unlikely(
         (info->nodeResult == -FhgfsOpsErr_COMMUNICATION) ||
         (info->nodeResult == -FhgfsOpsErr_AGAIN &&
            (context->ops->retryFlags & CK_RETRY_LOOP_EAGAIN) ) ) )
   { // comm error occurred => check whether we can do a retry
      if (fatal_signal_pending(current))
         info->nodeResult = -FhgfsOpsErr_INTERRUPTED;
      else if (App_getConnRetriesEnabled(context->app) &&
            (!context->maxNumRetries || context->currentRetryNum < context->maxNumRetries))
      { // we have retries left
         context->numRetryWaiters++;
         info->state = CommKitState_RETRYWAIT;
         return;
      }
   }

   // success or no retries left => done
   context->numDone++;
   info->state = CommKitState_DONE;
}

static void __commkit_start_retry(CommKitContext* context, int flags)
{
   struct CommKitTargetInfo* info;
   TargetStateStore* stateStore = App_getTargetStateStore(context->app);
   MirrorBuddyGroupMapper* mirrorBuddies = App_getStorageBuddyGroupMapper(context->app);
   unsigned patternType = context->ioInfo
      ? StripePattern_getPatternType(context->ioInfo->pattern)
      : STRIPEPATTERN_Invalid;

   bool cancelRetries = false; // true if there are offline targets
   bool resetRetries = false; /* true to not deplete retries if there are unusable target states
                                 ("!good && !offline") */
   bool sleepOnResetRetries = true; // true to retry immediately without sleeping

   // reset context values for retry round

   context->numRetryWaiters = 0;
   context->pollTimedOut = false;

   // check for offline targets

   list_for_each_entry(info, context->targetInfoList, targetInfoList)
   {
      if(info->state == CommKitState_RETRYWAIT)
      {
         CombinedTargetState targetState;
         CombinedTargetState buddyTargetState;
         uint16_t targetID = info->targetID;
         uint16_t buddyTargetID = info->targetID;
         bool getTargetStateRes;
         bool getBuddyTargetStateRes = true;

         // resolve the actual targetID

         if(patternType == STRIPEPATTERN_BuddyMirror)
         {
            targetID = info->useBuddyMirrorSecond ?
               MirrorBuddyGroupMapper_getSecondaryTargetID(mirrorBuddies, info->targetID) :
               MirrorBuddyGroupMapper_getPrimaryTargetID(mirrorBuddies, info->targetID);
            buddyTargetID = info->useBuddyMirrorSecond ?
               MirrorBuddyGroupMapper_getPrimaryTargetID(mirrorBuddies, info->targetID) :
               MirrorBuddyGroupMapper_getSecondaryTargetID(mirrorBuddies, info->targetID);
         }

         // check current target state

         getTargetStateRes = TargetStateStore_getState(stateStore, targetID,
            &targetState);
         if (targetID == buddyTargetID)
            buddyTargetState = targetState;
         else
            getBuddyTargetStateRes = TargetStateStore_getState(stateStore, buddyTargetID,
               &buddyTargetState);

         if( (!getTargetStateRes && ( (targetID != buddyTargetID) && !getBuddyTargetStateRes) ) ||
               ( (targetState.reachabilityState == TargetReachabilityState_OFFLINE) &&
               (buddyTargetState.reachabilityState == TargetReachabilityState_OFFLINE) ) )
         { // no more retries when both buddies are offline
            LOG_DEBUG_FORMATTED(context->log, Log_SPAM, context->ops->logContext,
               "Skipping communication with offline targetID: %hu",
               targetID);
            cancelRetries = true;
            break;
         }

         if(flags & CK_RETRY_BUDDY_FALLBACK)
         {
            if( ( !getTargetStateRes ||
                  (targetState.consistencyState != TargetConsistencyState_GOOD) ||
                  (targetState.reachabilityState != TargetReachabilityState_ONLINE) )
               && ( getBuddyTargetStateRes &&
                  (buddyTargetState.consistencyState == TargetConsistencyState_GOOD) &&
                  (buddyTargetState.reachabilityState == TargetReachabilityState_ONLINE) ) )
            { // current target not good but buddy is good => switch to buddy
               LOG_DEBUG_FORMATTED(context->log, Log_SPAM, context->ops->logContext,
                  "Switching to buddy with good target state. "
                  "targetID: %hu; target state: %s / %s",
                  targetID, TargetStateStore_reachabilityStateToStr(targetState.reachabilityState),
                  TargetStateStore_consistencyStateToStr(targetState.consistencyState) );
               info->useBuddyMirrorSecond = !info->useBuddyMirrorSecond;
               info->state = CommKitState_PREPARE;
               resetRetries = true;
               sleepOnResetRetries = false;
               continue;
            }
         }

         if( (patternType == STRIPEPATTERN_BuddyMirror) &&
            ( (targetState.reachabilityState != TargetReachabilityState_ONLINE) ||
            (targetState.consistencyState != TargetConsistencyState_GOOD) ) )
         { // both buddies not good, but at least one of them not offline => wait for clarification
            LOG_DEBUG_FORMATTED(context->log, Log_DEBUG, context->ops->logContext,
               "Waiting because of target state. "
               "targetID: %hu; target state: %s / %s",
               targetID, TargetStateStore_reachabilityStateToStr(targetState.reachabilityState),
               TargetStateStore_consistencyStateToStr(targetState.consistencyState) );
            info->state = CommKitState_PREPARE;
            resetRetries = true;
            continue;
         }

         if(info->nodeResult == -FhgfsOpsErr_AGAIN && (flags & CK_RETRY_LOOP_EAGAIN))
         {
            Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
               "Waiting because target asked for infinite retries. targetID: %hu", targetID);
            info->state = CommKitState_PREPARE;
            resetRetries = true;
            continue;
         }

         // normal retry
         info->state = CommKitState_PREPARE;
      }
   }

   // if we have offline targets, cancel all further retry waiters
   if(cancelRetries)
   {
      list_for_each_entry(info, context->targetInfoList, targetInfoList)
      {
         if( (info->state != CommKitState_RETRYWAIT) && (info->state != CommKitState_PREPARE ) )
            continue;

         context->numDone++;
         info->state = CommKitState_DONE;
      }

      return;
   }


   // wait before we actually start the retry
   if(resetRetries)
   { // reset retries to not deplete them in case of non-good and non-offline targets
      if(sleepOnResetRetries)
         Thread_sleep(COMMKIT_RESET_SLEEP_MS);

      context->currentRetryNum = 0;
   }
   else
   { // normal retry
      MessagingTk_waitBeforeRetry(context->currentRetryNum);

      context->currentRetryNum++;
   }
}

static FhgfsOpsErr __commkit_message_genericResponse(CommKitContext* context,
   struct CommKitTargetInfo* info, unsigned requestMsgType)
{
   const char* logContext = "Messaging (RPC)";

   bool parseRes;
   GenericResponseMsg msg;

   GenericResponseMsg_init(&msg);

   parseRes = NetMessageFactory_deserializeFromBuf(context->app, info->headerBuffer,
      info->headerSize, &msg.simpleIntStringMsg.netMessage, NETMSGTYPE_GenericResponse);
   if(!parseRes)
   {
      Logger_logFormatted(context->log, Log_ERR, "received bad message type from %s: %i",
         Node_getNodeIDWithTypeStr(info->node),
         NetMessage_getMsgType(&msg.simpleIntStringMsg.netMessage) );
      return -FhgfsOpsErr_INTERNAL;
   }

   switch(GenericResponseMsg_getControlCode(&msg) )
   {
      case GenericRespMsgCode_TRYAGAIN:
         if(!info->logged.peerTryAgain)
         {
            info->logged.peerTryAgain = true;

            Logger_logFormatted(context->log, Log_NOTICE, logContext,
               "Peer is asking for a retry: %s; Reason: %s",
               Node_getNodeIDWithTypeStr(info->node),
               GenericResponseMsg_getLogStr(&msg) );
            Logger_logFormatted(context->log, Log_DEBUG, logContext,
               "Message type: %u", requestMsgType);
         }

         return FhgfsOpsErr_AGAIN;

      case GenericRespMsgCode_INDIRECTCOMMERR:
         if(!info->logged.indirectCommError)
         {
            info->logged.indirectCommError = true;

            Logger_logFormatted(context->log, Log_NOTICE, logContext,
               "Peer reported indirect communication error: %s; Reason: %s",
               Node_getNodeIDWithTypeStr(info->node),
               GenericResponseMsg_getLogStr(&msg) );
            Logger_logFormatted(context->log, Log_DEBUG, logContext,
               "Message type: %u", requestMsgType);
         }

         return FhgfsOpsErr_COMMUNICATION;

      default:
         Logger_logFormatted(context->log, Log_NOTICE, logContext,
            "Peer replied with unknown control code: %s; Code: %u; Reason: %s",
            Node_getNodeIDWithTypeStr(info->node),
            (unsigned)GenericResponseMsg_getControlCode(&msg),
            GenericResponseMsg_getLogStr(&msg) );
         Logger_logFormatted(context->log, Log_DEBUG, logContext,
            "Message type: %u", requestMsgType);

         return FhgfsOpsErr_INTERNAL;
   }
}

void FhgfsOpsCommkit_communicate(App* app, RemotingIOInfo* ioInfo, struct list_head* targetInfos,
   const struct CommKitContextOps* ops, void* private)
{
   Config* cfg = App_getConfig(app);
   int numStates = 0;

   CommKitContext context =
   {
      .ops = ops,

      .app = app,
      .log = App_getLogger(app),
      .private = private,

      .ioInfo = ioInfo,

      .targetInfoList = targetInfos,

      .numRetryWaiters = 0, // counter for states that encountered a comm error
      .numDone = 0, // counter for finished states
      .numAcquiredConns = 0, // counter for currently acquired conns (to wait only for first conn)

      .pollTimedOut = false,
      .pollTimeoutLogged = false,
      .connFailedLogged = false,

      .currentRetryNum = 0,
      .maxNumRetries = Config_getConnNumCommRetries(cfg),
   };

   do
   {
      struct CommKitTargetInfo* info;

      context.numUnconnectable = 0; // will be increased by states the didn't get a conn
      context.numPollSocks = 0; // will be increased by the states that need to poll
      context.numBufferless = 0;
      numStates = 0;

      PollState_init(&context.pollState);

      // let each state do something (before we call poll)
      list_for_each_entry(info, targetInfos, targetInfoList)
      {
         numStates++;

         switch(info->state)
         {
            case CommKitState_PREPARE:
               if(!__commkit_prepare_generic(&context, info) )
                  break;
               /* fall through */

            case CommKitState_SENDHEADER:
               __commkit_sendheader_generic(&context, info);
               __commkit_add_socket_pollstate(&context, info,
                  context.ops->sendData ? POLLOUT : POLLIN);
               break;

            case CommKitState_SENDDATA:
               if(context.ops->sendData)
               {
                   __commkit_senddata_generic(&context, info);
                  break;
               }
               /* fall-through -if no sendData state is set */

            case CommKitState_RECVHEADER:
               if(context.ops->recvHeader)
               {
                  __commkit_recvheader_generic(&context, info);
                  break;
               }
               /* fall-through -if no recvHeader state is set */

            case CommKitState_RECVDATA:
               if(context.ops->recvData)
               {
                  __commkit_recvdata_generic(&context, info);
                  break;
               }
               /* fall-through -if no recvData state is set */

            case CommKitState_CLEANUP:
               __commkit_cleanup_generic(&context, info);
               break;

            case CommKitState_SOCKETINVALIDATE:
               __commkit_socketinvalidate_generic(&context, info);
               __commkit_cleanup_generic(&context, info);
               break;

            case CommKitState_RETRYWAIT:
            case CommKitState_DONE:
               // nothing to be done here
               break;

            default:
               BUG();
         }
      }

      if(context.numPollSocks)
         __FhgfsOpsCommKitCommon_pollStateSocks(&context, numStates);
      else
      if(unlikely(
         context.numRetryWaiters &&
         ( (context.numDone + context.numRetryWaiters) == numStates) ) )
      { // we have some retry waiters and the rest is done
         // note: this can only happen if we don't have any pollsocks, hence the "else"

         __commkit_start_retry(&context, context.ops->retryFlags);
      }
   } while(context.numDone != numStates);
}



static unsigned __commkit_readfile_prepareHeader(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   Config* cfg = App_getConfig(context->app);
   Node* localNode = App_getLocalNode(context->app);
   const NumNodeID localNodeNumID = Node_getNumID(localNode);
   ReadfileState* currentState = container_of(info, ReadfileState, base);
   struct ReadfileIterOps* ops = context->private;
   ReadLocalFileV2Msg readMsg;

   // prepare message
   ReadLocalFileV2Msg_initFromSession(&readMsg, localNodeNumID,
      context->ioInfo->fileHandleID, info->targetID, context->ioInfo->pathInfo,
      context->ioInfo->accessFlags, currentState->offset, currentState->totalReadSize);

   NetMessage_setMsgHeaderTargetID(&readMsg.netMessage, info->selectedTargetID);

   if (currentState->firstWriteDoneForTarget && Config_getSysSessionChecksEnabled(cfg))
      NetMessage_addMsgHeaderFeatureFlag(&readMsg.netMessage, READLOCALFILEMSG_FLAG_SESSION_CHECK);

   if(StripePattern_getPatternType(context->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   {
      NetMessage_addMsgHeaderFeatureFlag(&readMsg.netMessage, READLOCALFILEMSG_FLAG_BUDDYMIRROR);

      if(info->useBuddyMirrorSecond)
         NetMessage_addMsgHeaderFeatureFlag(&readMsg.netMessage,
            READLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   if(App_getNetBenchModeEnabled(context->app) )
      NetMessage_addMsgHeaderFeatureFlag(&readMsg.netMessage, READLOCALFILEMSG_FLAG_DISABLE_IO);

   NetMessage_serialize(&readMsg.netMessage, info->headerBuffer, BEEGFS_COMMKIT_MSGBUF_SIZE);

   currentState->transmitted = 0;
   currentState->toBeTransmitted = 0;
   currentState->data.count = 0;
   currentState->receiveFileData = false;

   if(ops->prepare)
      ops->prepare(context, currentState);

   return NetMessage_getMsgLength(&readMsg.netMessage);
}

static void __commkit_readfile_printSocketDetails(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   ReadfileState* currentState = container_of(info, ReadfileState, base);

   Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
      "Sent request: node: %s; fileHandleID: %s; offset: %lld; size: %zu",
      Node_getNodeIDWithTypeStr(info->node), context->ioInfo->fileHandleID,
      (long long)currentState->offset, currentState->totalReadSize);
}

static ssize_t __commkit_readfile_receive(CommKitContext* context, ReadfileState* currentState,
   void* buffer, size_t length, bool exact)
{
   ssize_t recvRes;
   Socket* socket = currentState->base.socket;

   Config* cfg = App_getConfig(context->app);

   if(BEEGFS_SHOULD_FAIL(commkit_readfile_receive_timeout, 1) )
      recvRes = -ETIMEDOUT;
   else
   if(exact)
      recvRes = Socket_recvExactT(socket, buffer, length, 0, cfg->connMsgLongTimeout);
   else
      recvRes = Socket_recvT(socket, buffer, length, 0, cfg->connMsgLongTimeout);

   if(unlikely(recvRes < 0) )
   {
      Logger_logFormatted(context->log, Log_SPAM, context->ops->logContext,
         "Request details: receive from %s: %lld bytes (error %zi)",
         Node_getNodeIDWithTypeStr(currentState->base.node), (long long)length, recvRes);
   }

   return recvRes;
}

static int __commkit_readfile_recvdata_prefix(CommKitContext* context, ReadfileState* currentState)
{
   ssize_t recvRes;
   char dataLenBuf[sizeof(int64_t)]; // length info in fhgfs network byte order
   int64_t lengthInfo; // length info in fhgfs host byte order
   DeserializeCtx ctx = { dataLenBuf, sizeof(int64_t) };

   recvRes = __commkit_readfile_receive(context, currentState, dataLenBuf, sizeof(int64_t), true);
   if(recvRes < 0)
      return recvRes;
   if (recvRes == 0)
      return -ECOMM;

   // got the length info response
   Serialization_deserializeInt64(&ctx, &lengthInfo);

   if(lengthInfo <= 0)
   { // end of file data transmission
      if(unlikely(lengthInfo < 0) )
      { // error occurred
         currentState->base.nodeResult = lengthInfo;
      }
      else
      { // normal end of file data transmission
         currentState->base.nodeResult = currentState->transmitted;
      }

      return 0;
   }

   // buffer overflow check
   if(unlikely(currentState->transmitted + lengthInfo > currentState->totalReadSize) )
   {
      Logger_logErrFormatted(context->log, context->ops->logContext,
         "Bug: Received a lengthInfo that would overflow request from %s: %lld %zu %zu",
         Node_getNodeIDWithTypeStr(currentState->base.node), (long long)lengthInfo,
         currentState->transmitted, currentState->totalReadSize);

      return -EREMOTEIO;
   }

   // positive result => node is going to send some file data
   currentState->toBeTransmitted += lengthInfo;
   currentState->receiveFileData = true;
   return 1;
}

static int __commkit_readfile_recvdata(CommKitContext* context, struct CommKitTargetInfo* info)
{
   ReadfileState* currentState = container_of(info, ReadfileState, base);
   size_t missingLength;
   ssize_t recvRes;
   struct iovec iov;

   if(!currentState->receiveFileData)
      return __commkit_readfile_recvdata_prefix(context, currentState);

   if(currentState->data.count == 0)
   {
      ((struct ReadfileIterOps*) context->private)->nextIter(context, currentState);
      BUG_ON(currentState->data.count == 0);
   }

   iov = iov_iter_iovec(&currentState->data);
   missingLength = MIN(iov.iov_len, currentState->toBeTransmitted - currentState->transmitted);

   // receive available dataPart
   recvRes = __commkit_readfile_receive(context, currentState, iov.iov_base, missingLength, false);
   if(recvRes < 0)
      return recvRes;

   currentState->transmitted += recvRes;
   iov_iter_advance(&currentState->data, recvRes);

   if(currentState->toBeTransmitted == currentState->transmitted)
   { // all of the data has been received => receive the next lengthInfo in the header stage
      currentState->receiveFileData = false;
   }

   return 1;
}

static const struct CommKitContextOps readfileOps = {
   .prepareHeader = __commkit_readfile_prepareHeader,
   .recvData = __commkit_readfile_recvdata,

   .printSocketDetails = __commkit_readfile_printSocketDetails,

   .retryFlags = CK_RETRY_BUDDY_FALLBACK,
   .logContext = "readfileV2 (communication)",
};

void FhgfsOpsCommKit_readfileV2bCommunicate(App* app, RemotingIOInfo* ioInfo,
   struct list_head* states, void (*nextIter)(CommKitContext*, ReadfileState*),
   void (*prepare)(CommKitContext*, ReadfileState*))
{
   struct ReadfileIterOps iops = {
      .nextIter = nextIter,
      .prepare = prepare,
   };

   FhgfsOpsCommkit_communicate(app, ioInfo, states, &readfileOps, &iops);
}



static unsigned __commkit_writefile_prepareHeader(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   Config* cfg = App_getConfig(context->app);
   Node* localNode = App_getLocalNode(context->app);
   const NumNodeID localNodeNumID = Node_getNumID(localNode);
   WritefileState* currentState = container_of(info, WritefileState, base);
   struct WritefileIterOps* ops = context->private;

   WriteLocalFileMsg writeMsg;

   // prepare message
   WriteLocalFileMsg_initFromSession(&writeMsg, localNodeNumID,
      context->ioInfo->fileHandleID, info->targetID, context->ioInfo->pathInfo,
      context->ioInfo->accessFlags, currentState->offset, currentState->totalWriteSize);

   NetMessage_setMsgHeaderTargetID(&writeMsg.netMessage, info->selectedTargetID);

   if (Config_getQuotaEnabled(cfg) )
      WriteLocalFileMsg_setUserdataForQuota(&writeMsg, context->ioInfo->userID,
         context->ioInfo->groupID);

   if (currentState->firstWriteDoneForTarget && Config_getSysSessionChecksEnabled(cfg))
      NetMessage_addMsgHeaderFeatureFlag(&writeMsg.netMessage,
         WRITELOCALFILEMSG_FLAG_SESSION_CHECK);

   if(StripePattern_getPatternType(context->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   {
      NetMessage_addMsgHeaderFeatureFlag(&writeMsg.netMessage, WRITELOCALFILEMSG_FLAG_BUDDYMIRROR);

      NetMessage_addMsgHeaderFeatureFlag(&writeMsg.netMessage,
         WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_FORWARD);

      if(info->useBuddyMirrorSecond)
         NetMessage_addMsgHeaderFeatureFlag(&writeMsg.netMessage,
            WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   if(App_getNetBenchModeEnabled(context->app) )
      NetMessage_addMsgHeaderFeatureFlag(&writeMsg.netMessage, WRITELOCALFILEMSG_FLAG_DISABLE_IO);

   NetMessage_serialize(&writeMsg.netMessage, info->headerBuffer, BEEGFS_COMMKIT_MSGBUF_SIZE);

   currentState->transmitted = 0;
   currentState->toBeTransmitted = currentState->totalWriteSize;
   currentState->data.count = 0;

   if(ops->prepare)
      ops->prepare(context, currentState);

   return NetMessage_getMsgLength(&writeMsg.netMessage);
}

static int __commkit_writefile_sendData(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   WritefileState* currentState = container_of(info, WritefileState, base);
   ssize_t sendRes = 0;
   struct iov_iter* data = &currentState->data;
   size_t partLen = data->count;

   if(currentState->toBeTransmitted == 0)
      return 0;

   if(data->count == 0)
   {
      ((struct WritefileIterOps*) context->private)->nextIter(context, currentState);
      BUG_ON(data->count == 0);
   }

   if(BEEGFS_SHOULD_FAIL(commkit_writefile_senddata_timeout, 1) )
   {
      sendRes = -ETIMEDOUT;
      goto sendsDone;
   }

   while(data->count)
   {
      sendRes = info->socket->ops->sendto(info->socket, data, MSG_DONTWAIT, NULL);
      if (sendRes < 0)
         break;

      currentState->toBeTransmitted -= sendRes;
      currentState->transmitted += sendRes;
   }

sendsDone:
   if(sendRes < 0 && sendRes != -EAGAIN)
   {
      if(sendRes != -EFAULT)
      {
         // log errors the user probably hasn't caused
         Logger_logFormatted(context->log, Log_WARNING, context->ops->logContext,
            "SocketError. ErrCode: %zd", sendRes);
         Logger_logFormatted(context->log, Log_SPAM, context->ops->logContext,
            "partLen/missing: %zd/%zd", partLen, data->count);
      }

      return sendRes;
   }

   if(currentState->toBeTransmitted == 0)
      return 0;

   return 1;
}

static void __commkit_writefile_printSendDataDetails(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   WritefileState* currentState = container_of(info, WritefileState, base);

   Logger_logFormatted(context->log, Log_SPAM, context->ops->logContext,
      "Request details: transmitted/toBeTransmitted: %lld/%lld",
      (long long)currentState->transmitted, (long long)currentState->toBeTransmitted);
}

static void __commkit_writefile_printSocketDetails(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   WritefileState* currentState = container_of(info, WritefileState, base);

   Logger_logFormatted(context->log, Log_DEBUG, context->ops->logContext,
      "Sent request: node: %s; fileHandleID: %s; offset: %lld; size: %lld",
      Node_getNodeIDWithTypeStr(info->node), context->ioInfo->fileHandleID,
      (long long)currentState->offset, (long long)currentState->toBeTransmitted);
}

static int __commkit_writefile_recvHeader(CommKitContext* context, struct CommKitTargetInfo* info)
{
   WritefileState* currentState = container_of(info, WritefileState, base);
   bool deserRes;
   int64_t writeRespValue;
   WriteLocalFileRespMsg writeRespMsg;

   // got response => deserialize it
   WriteLocalFileRespMsg_init(&writeRespMsg);

   deserRes = NetMessageFactory_deserializeFromBuf(context->app, currentState->base.headerBuffer,
      info->headerSize, (NetMessage*)&writeRespMsg, NETMSGTYPE_WriteLocalFileResp);

   if(unlikely(!deserRes) )
   { // response invalid
      Logger_logFormatted(context->log, Log_WARNING, context->ops->logContext,
         "Received invalid response from %s. Expected type: %d. Disconnecting: %s",
         Node_getNodeIDWithTypeStr(currentState->base.node), NETMSGTYPE_WriteLocalFileResp,
         Socket_getPeername(currentState->base.socket) );

      return -FhgfsOpsErr_COMMUNICATION;
   }

   // correct response
   writeRespValue = WriteLocalFileRespMsg_getValue(&writeRespMsg);

   if(unlikely(writeRespValue == -FhgfsOpsErr_COMMUNICATION) && !context->connFailedLogged)
   { // server was unable to communicate with another server
      context->connFailedLogged = true;
      Logger_logFormatted(context->log, Log_WARNING, context->ops->logContext,
         "Server reported indirect communication error: %s. targetID: %hu",
         Node_getNodeIDWithTypeStr(currentState->base.node), currentState->base.targetID);
   }

   currentState->base.nodeResult = writeRespValue;
   return 0;
}

static const struct CommKitContextOps writefileOps = {
   .prepareHeader = __commkit_writefile_prepareHeader,
   .sendData = __commkit_writefile_sendData,
   .recvHeader = __commkit_writefile_recvHeader,

   .printSendDataDetails = __commkit_writefile_printSendDataDetails,
   .printSocketDetails = __commkit_writefile_printSocketDetails,

   .retryFlags = CK_RETRY_LOOP_EAGAIN,
   .logContext = "writefile (communication)",
};

void FhgfsOpsCommKit_writefileV2bCommunicate(App* app, RemotingIOInfo* ioInfo,
   struct list_head* states, void (*nextIter)(CommKitContext*, WritefileState*),
   void (*prepare)(CommKitContext*, WritefileState*))
{
   struct WritefileIterOps iops = {
      .nextIter = nextIter,
      .prepare = prepare,
   };

   FhgfsOpsCommkit_communicate(app, ioInfo, states, &writefileOps, &iops);
}



static enum CKTargetBadAction __commkit_fsync_selectedTargetBad(CommKitContext* context,
   struct CommKitTargetInfo* info, const CombinedTargetState* targetState)
{
   // we must not try to fsync a secondary that is currently offline, but we should fsync
   // secondaries that are needs-resync (because a resync might be running, but our file was already
   // resynced). we will also try to fsync a secondary that is poffline, just in case it comes back
   // to online.
   // bad targets should be silently ignored just like offline targets because they might do
   // who-knows-what with our request and produce spurious errors.
   if (StripePattern_getPatternType(context->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror &&
         info->useBuddyMirrorSecond)
   {
      if (targetState->reachabilityState == TargetReachabilityState_OFFLINE ||
            targetState->consistencyState == TargetConsistencyState_BAD)
         info->nodeResult = -FhgfsOpsErr_SUCCESS;
      else if (targetState->consistencyState == TargetConsistencyState_NEEDS_RESYNC)
         return CK_CONTINUE_TARGET;
   }

   return CK_SKIP_TARGET;
}

static unsigned __commkit_fsync_prepareHeader(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   struct FsyncContext* fctx = context->private;
   struct FsyncState* state = container_of(info, struct FsyncState, base);

   Config* cfg = App_getConfig(context->app);
   Node* localNode = App_getLocalNode(context->app);
   const NumNodeID localNodeNumID = Node_getNumID(localNode);

   FSyncLocalFileMsg requestMsg;

   if(!fctx->forceRemoteFlush && !fctx->checkSession && !fctx->doSyncOnClose)
   {
      info->nodeResult = -FhgfsOpsErr_SUCCESS;
      return 0;
   }

   // prepare request message
   FSyncLocalFileMsg_initFromSession(&requestMsg, localNodeNumID, context->ioInfo->fileHandleID,
      info->targetID);

   NetMessage_setMsgHeaderUserID(&requestMsg.netMessage, fctx->userID);
   NetMessage_setMsgHeaderTargetID(&requestMsg.netMessage, info->selectedTargetID);

   if(StripePattern_getPatternType(context->ioInfo->pattern) == STRIPEPATTERN_BuddyMirror)
   {
      NetMessage_addMsgHeaderFeatureFlag(&requestMsg.netMessage,
         FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR);

      if(info->useBuddyMirrorSecond)
         NetMessage_addMsgHeaderFeatureFlag(&requestMsg.netMessage,
            FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND);
   }

   // required version is checked in FSyncChunkFileWork_process and if session check is not
   // supported by the server, the value of this->checkSession was set to false
   if(fctx->checkSession && state->firstWriteDoneForTarget
         && Config_getSysSessionChecksEnabled(cfg))
      NetMessage_addMsgHeaderFeatureFlag(&requestMsg.netMessage,
         FSYNCLOCALFILEMSG_FLAG_SESSION_CHECK);

   // required version is checked in FSyncChunkFileWork_process and if syncOnClose is not
   // supported by the server, the value of this->doSyncOnClose was set to false
   if(!fctx->forceRemoteFlush && !fctx->doSyncOnClose)
      NetMessage_addMsgHeaderFeatureFlag(&requestMsg.netMessage,
         FSYNCLOCALFILEMSG_FLAG_NO_SYNC);

   NetMessage_serialize(&requestMsg.netMessage, info->headerBuffer, BEEGFS_COMMKIT_MSGBUF_SIZE);

   return NetMessage_getMsgLength(&requestMsg.netMessage);
}

static int __commkit_fsync_recvHeader(CommKitContext* context, struct CommKitTargetInfo* info)
{
   FSyncLocalFileRespMsg respMsg;
   bool parseRes;

   FSyncLocalFileRespMsg_init(&respMsg);

   parseRes = NetMessageFactory_deserializeFromBuf(context->app, info->headerBuffer,
      info->headerSize, &respMsg.simpleInt64Msg.netMessage, NETMSGTYPE_FSyncLocalFileResp);
   if(!parseRes)
      return __commkit_message_genericResponse(context, info, NETMSGTYPE_FSyncLocalFile);

   info->nodeResult = -FSyncLocalFileRespMsg_getValue(&respMsg);
   return 0;
}

static const struct CommKitContextOps fsyncOps = {
   .selectedTargetBad = __commkit_fsync_selectedTargetBad,
   .prepareHeader = __commkit_fsync_prepareHeader,
   .recvHeader = __commkit_fsync_recvHeader,

   .retryFlags = CK_RETRY_LOOP_EAGAIN,
   .logContext = "fsync (communication)",
};

void FhgfsOpsCommKit_fsyncCommunicate(App* app, RemotingIOInfo* ioInfo,
   struct FsyncContext* context)
{
   FhgfsOpsCommkit_communicate(app, ioInfo, &context->states, &fsyncOps, context);
}



static unsigned __commkit_statstorage_prepareHeader(CommKitContext* context,
   struct CommKitTargetInfo* info)
{
   StatStoragePathMsg msg;

   StatStoragePathMsg_initFromTarget(&msg, info->targetID);
   NetMessage_setMsgHeaderTargetID(&msg.simpleUInt16Msg.netMessage, info->selectedTargetID);

   NetMessage_serialize(&msg.simpleUInt16Msg.netMessage, info->headerBuffer,
      BEEGFS_COMMKIT_MSGBUF_SIZE);

   return NetMessage_getMsgLength(&msg.simpleUInt16Msg.netMessage);
}

static int __commkit_statstorage_recvHeader(CommKitContext* context, struct CommKitTargetInfo* info)
{
   struct StatStorageState* state = container_of(info, struct StatStorageState, base);

   StatStoragePathRespMsg respMsg;
   bool parseRes;

   StatStoragePathRespMsg_init(&respMsg);

   parseRes = NetMessageFactory_deserializeFromBuf(context->app, info->headerBuffer,
      info->headerSize, &respMsg.netMessage, NETMSGTYPE_StatStoragePathResp);
   if(!parseRes)
      return __commkit_message_genericResponse(context, info, NETMSGTYPE_StatStoragePath);

   state->totalFree = respMsg.sizeFree;
   state->totalSize = respMsg.sizeTotal;
   info->nodeResult = -respMsg.result;
   return 0;
}

static const struct CommKitContextOps statstorageOps = {
   .prepareHeader = __commkit_statstorage_prepareHeader,
   .recvHeader = __commkit_statstorage_recvHeader,

   .retryFlags = CK_RETRY_LOOP_EAGAIN,
   .logContext = "stat storage (communication)",
};

void FhgfsOpsCommKit_statStorageCommunicate(App* app, struct list_head* targets)
{
   FhgfsOpsCommkit_communicate(app, NULL, targets, &statstorageOps, NULL);
}
