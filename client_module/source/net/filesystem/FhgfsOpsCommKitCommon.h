#ifndef FHGFSOPSCOMMKITCOMMON_H_
#define FHGFSOPSCOMMKITCOMMON_H_

#include <app/log/Logger.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SocketTk.h>
#include <common/Common.h>
#include <common/Types.h>
#include <net/filesystem/RemotingIOInfo.h>
#include <nodes/NodeStoreEx.h>


#define BEEGFS_COMMKIT_MSGBUF_SIZE         4096

#define COMMKIT_DEBUG_READ_SEND      (1 << 0)
#define COMMKIT_DEBUG_READ_HEADER    (1 << 1)
#define COMMKIT_DEBUG_RECV_DATA      (1 << 2)
#define COMMKIT_DEBUG_WRITE_HEADER   (1 << 3)
#define COMMKIT_DEBUG_WRITE_POLL     (1 << 4)
#define COMMKIT_DEBUG_WRITE_SEND     (1 << 5)
#define COMMKIT_DEBUG_WRITE_RECV     (1 << 6)

// #define BEEGFS_COMMKIT_DEBUG 0b010

#ifndef BEEGFS_COMMKIT_DEBUG
#define BEEGFS_COMMKIT_DEBUG 0
#endif

#ifdef BEEGFS_COMMKIT_DEBUG
#define CommKitErrorInjectRate   101 // fail a send/receive req every X jiffies
#endif



struct CommKitContext;
typedef struct CommKitContext CommKitContext;



static inline void __FhgfsOpsCommKitCommon_pollStateSocks(CommKitContext* context,
   size_t numStates);
static inline void __FhgfsOpsCommKitCommon_handlePollError(CommKitContext* context,
   int pollRes);


struct CommKitTargetInfo;

enum
{
   CK_RETRY_BUDDY_FALLBACK = 1 << 0,
   CK_RETRY_LOOP_EAGAIN = 1 << 1,
};

enum CKTargetBadAction
{
   CK_SKIP_TARGET = 0,
   CK_CONTINUE_TARGET = 1,
};


struct CommKitContextOps
{
   enum CKTargetBadAction (*selectedTargetBad)(CommKitContext*, struct CommKitTargetInfo*,
      const CombinedTargetState*);

   unsigned (*prepareHeader)(CommKitContext*, struct CommKitTargetInfo*);

   // returns >0 for successful send, 0 to advance to next state, or negative error code
   int (*sendData)(CommKitContext*, struct CommKitTargetInfo*);

   // return >= for success, 0 to advance, or negative beegfs error code
   int (*recvHeader)(CommKitContext*, struct CommKitTargetInfo*);
   // return >= for success, 0 to advance, or negative beegfs error code
   int (*recvData)(CommKitContext*, struct CommKitTargetInfo*);

   void (*printSendDataDetails)(CommKitContext*, struct CommKitTargetInfo*);
   void (*printSocketDetails)(CommKitContext*, struct CommKitTargetInfo*);

   int retryFlags;
   const char* logContext;
};


/**
 * Additional data that is required or useful for all the states.
 * (This is shared states data.)
 *
 * invariant: (numRetryWaiters + numDone + numUnconnectable + numPollSocks) <= numStates
 */
struct CommKitContext
{
   const struct CommKitContextOps* ops;

   App* app;
   Logger* log;
   void* private;

   RemotingIOInfo* ioInfo;

   struct list_head* targetInfoList;

   unsigned numRetryWaiters; // number of states with a communication error
   unsigned numDone; // number of finished states

   unsigned numAcquiredConns; // number of acquired conns to decide waiting or not (avoids deadlock)
   unsigned numUnconnectable; // states that didn't get a conn this round (reset to 0 in each round)
   unsigned numBufferless; // states that couldn't acquire a header buffer

   bool pollTimedOut;
   bool pollTimeoutLogged;
   bool connFailedLogged;

   PollState pollState;
   unsigned numPollSocks;

   unsigned currentRetryNum; // number of used up retries (in case of comm errors)
   unsigned maxNumRetries; // 0 if infinite number of retries enabled
};



/**
 * Poll the state socks that need to be polled.
 * This will call _handlePollError() if something goes wrong with the poll(), e.g. timeout.
 *
 * Note: Caller must have checked that there actually are socks that need polling
 *    (context.numPollSocks>0) before calling this.
 */
void __FhgfsOpsCommKitCommon_pollStateSocks(CommKitContext* context, size_t numStates)
{
   int pollRes;

   Config* cfg = App_getConfig(context->app);

   size_t numWaiters = context->numPollSocks + context->numRetryWaiters +
      context->numDone + context->numUnconnectable + context->numBufferless;

   /* if not all the states are polling or done, some states must be ready to do something
      immediately => set timeout to 0 (non-blocking) */
   /* note: be very careful with timeoutMS==0, because that means we don't release the CPU,
      so we need to ensure that timeoutMS==0 is no permanent condition. */
   int timeoutMS = (numWaiters < numStates) ? 0 : cfg->connMsgLongTimeout;
   BEEGFS_BUG_ON_DEBUG(numWaiters > numStates, "numWaiters > numStates should never happen");

   pollRes = SocketTk_poll(&context->pollState, timeoutMS);
   if(unlikely( (timeoutMS && pollRes <= 0) || (pollRes < 0) ) )
      __FhgfsOpsCommKitCommon_handlePollError(context, pollRes);
}

void __FhgfsOpsCommKitCommon_handlePollError(CommKitContext* context, int pollRes)
{
   // note: notifies the waiting sockets via context.pollTimedOut (they do not care whether
   // it was a timeout or an error)

   if(!context->pollTimeoutLogged)
   {
      if(!pollRes)
      { // no incoming data => timout or interrupted
         if (fatal_signal_pending(current))
         { // interrupted
            Logger_logFormatted(context->log, 3, context->ops->logContext,
               "Communication (poll() ) interrupted by signal.");
         }
         else
         { // timout
            Logger_logErrFormatted(context->log, context->ops->logContext,
               "Communication (poll() ) timeout for %u sockets.", context->numPollSocks);
         }
      }
      else
      { // error
         Logger_logErrFormatted(context->log, context->ops->logContext,
            "Communication (poll() ) error for %u sockets. ErrCode: %d",
            context->numPollSocks, pollRes);
      }
   }

   context->pollTimeoutLogged = true;
   context->pollTimedOut = true;
}


#endif /*FHGFSOPSCOMMKITCOMMON_H_*/
