#ifndef MESSAGINGTK_H_
#define MESSAGINGTK_H_

#include <app/log/Logger.h>
#include <app/App.h>
#include <app/config/Config.h>
#include <common/net/message/NetMessage.h>
#include <common/net/sock/Socket.h>
#include <common/nodes/MirrorBuddyGroup.h>
#include <common/nodes/Node.h>
#include <common/threading/Thread.h>
#include <common/toolkit/MessagingTkArgs.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include <net/message/NetMessageFactory.h>


extern FhgfsOpsErr MessagingTk_requestResponseWithRRArgs(App* app,
   RequestResponseArgs* rrArgs);
extern FhgfsOpsErr MessagingTk_requestResponseKMalloc(App* app, Node* node,
   NetMessage* requestMsg, unsigned respMsgType, char** outRespBuf, NetMessage** outRespMsg);
extern FhgfsOpsErr MessagingTk_requestResponse(App* app, Node* node,
   NetMessage* requestMsg, unsigned respMsgType, char** outRespBuf, NetMessage** outRespMsg);

extern FhgfsOpsErr MessagingTk_requestResponseRetry(App* app, Node* node,
   NetMessage* requestMsg, unsigned respMsgType, char** outRespBuf, NetMessage** outRespMsg);

extern FhgfsOpsErr MessagingTk_requestResponseNode(App* app,
   RequestResponseNode* rrNode, RequestResponseArgs* rrArgs);
extern FhgfsOpsErr MessagingTk_requestResponseNodeRetryAutoIntr(App* app,
   RequestResponseNode* rrNode, RequestResponseArgs* rrArgs);

// private

extern FhgfsOpsErr __MessagingTk_requestResponseWithRRArgsComm(App* app,
   RequestResponseArgs* rrArgs, MirrorBuddyGroup* group, bool* wasIndirectCommErr);
extern FhgfsOpsErr __MessagingTk_handleGenericResponse(App* app, RequestResponseArgs* rrArgs,
   MirrorBuddyGroup* group, bool* wasIndirectCommErr);

extern FhgfsOpsErr __MessagingTk_requestResponseNodeRetry(App* app,
   RequestResponseNode* rrNode, RequestResponseArgs* rrArgs);

// inliners

static inline char* MessagingTk_createMsgBuf(NetMessage* msg);
static inline ssize_t MessagingTk_recvMsgBuf(App* app, Socket* sock, char* bufIn, size_t bufInLen);

static inline void MessagingTk_waitBeforeRetry(unsigned currentNumRetry);
static inline int MessagingTk_getRetryWaitMS(unsigned currentNumRetry);



/**
 * Creates a message buffer of the required size and serializes the message to it.
 * Note: Uses kmalloc, so this is only safe for messages < 128kB
 *
 * @return buffer must be kfreed by the caller
 */
char* MessagingTk_createMsgBuf(NetMessage* msg)
{
   size_t bufLen = NetMessage_getMsgLength(msg);
   char* buf = (char*)os_kmalloc(bufLen);

   NetMessage_serialize(msg, buf, buf ? bufLen : 0);

   return buf;
}

/**
 * Receive a complete message into given buffer, extended version.
 *
 * Note: This is the version for pre-allocated buffers.
 *
 * @param bufIn incoming message buffer.
 * @return positive message length on success, <=0 on error (e.g. -ETIMEDOUT on recv timeout,
 * -EMSGSIZE if msg too large for buffer).
 */
ssize_t MessagingTk_recvMsgBuf(App* app, Socket* sock, char* bufIn, size_t bufInLen)
{
   const char* logContext = "MessagingTk (recv msg)";

   size_t numReceived = 0;
   ssize_t recvRes;
   size_t msgLength;
   Config* cfg = App_getConfig(app);

   // receive at least the message header

   recvRes = Socket_recvExactTEx(sock, bufIn, NETMSG_MIN_LENGTH, 0, cfg->connMsgLongTimeout,
         &numReceived);

   if(unlikely(recvRes <= 0) )
   { // socket error
      Logger* log = App_getLogger(app);
      Logger_logFormatted(log, Log_DEBUG, logContext, "Failed to receive message header from: %s",
         Socket_getPeername(sock) );

      goto socket_exception;
   }

   msgLength = NetMessage_extractMsgLengthFromBuf(bufIn);

   if(msgLength <= numReceived)
      return msgLength; // success (msg had no additional payload)


   // receive the message payload part

   if(unlikely(msgLength > bufInLen) )
   { // message too big to be accepted
      Logger* log = App_getLogger(app);
      Logger_logFormatted(log, Log_WARNING, logContext,
         "Received a message that is too large from: %s (bufLen: %lld, msgLen: %lld)",
         Socket_getPeername(sock), (long long)bufInLen, (long long)msgLength);

      return -EMSGSIZE;
   }

   recvRes = Socket_recvExactTEx(sock, &bufIn[numReceived], msgLength-numReceived, 0,
         cfg->connMsgLongTimeout, &numReceived);

   if(unlikely(recvRes <= 0) )
      goto socket_exception;

   // success
   return msgLength;


   socket_exception:
   {
      Logger* log = App_getLogger(app);

      if (fatal_signal_pending(current))
         Logger_logFormatted(log, Log_DEBUG, logContext,
            "Receive interrupted by signal");
      else
         Logger_logFormatted(log, Log_DEBUG, logContext,
            "Receive failed from: %s (ErrCode: %lld)",
            Socket_getPeername(sock), (long long)recvRes);
   }

   return recvRes;
}

void MessagingTk_waitBeforeRetry(unsigned currentNumRetry)
{
   int retryWaitMS = MessagingTk_getRetryWaitMS(currentNumRetry);

   if(retryWaitMS)
      Thread_sleep(retryWaitMS);
}

int MessagingTk_getRetryWaitMS(unsigned currentNumRetry)
{
   // note: keep in sync with __Config_initConnNumCommRetries()

   int retryWaitMS ;

   if(!currentNumRetry)
   {
      retryWaitMS = 0;
   }
   else
   if(currentNumRetry <= 12) // 1st minute
   {
      retryWaitMS = 5000;
   }
   else
   if(currentNumRetry <= 24) // 2nd to 5th minute
   {
      retryWaitMS = 20000;
   }
   else // after 5th minute
   {
      retryWaitMS = 60000;
   }

   return retryWaitMS;
}

#endif /*MESSAGINGTK_H_*/
