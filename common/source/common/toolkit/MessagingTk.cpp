#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/net/message/NetMessageLogHelper.h>
#include <common/threading/PThread.h>
#include <common/toolkit/DebugVariable.h>
#include "MessagingTk.h"


#define MSGBUF_DEFAULT_SIZE     (64*1024) // at least big enough to store a datagram
#define MSGBUF_MAX_SIZE         (4*1024*1024) // max accepted size


DEBUG_ENV_VAR(unsigned, RECEIVE_TIMEOUT, CONN_LONG_TIMEOUT, "BEEGFS_MESSAGING_RECV_TIMEOUT_MS");

bool MessagingTk::requestResponse(RequestResponseArgs* rrArgs)
{
   FhgfsOpsErr commRes = requestResponseComm(rrArgs);
   // One retry in case the connection was already broken when we got it (e.g. peer daemon restart).
   if (commRes == FhgfsOpsErr_COMMUNICATION)
   {
      LOG(COMMUNICATION, WARNING, "Retrying communication.",
           as("peer",  rrArgs->node->getNodeIDWithTypeStr()),
           as("message type", rrArgs->requestMsg->getMsgTypeStr()));
      commRes = requestResponseComm(rrArgs);
   }

   return commRes == FhgfsOpsErr_SUCCESS;
}

/**
 * Sends a message and receives the response.
 *
 * @param outRespBuf the underlying buffer for outRespMsg needs to be freed by the caller
 * if true is returned.
 * @param outRespMsg response message; needs to be freed by the caller if true is returned.
 * @return true if communication succeeded and expected response was received
 */
bool MessagingTk::requestResponse(Node& node, NetMessage* requestMsg, unsigned respMsgType,
   char** outRespBuf, NetMessage** outRespMsg)
{
   RequestResponseArgs rrArgs(&node, requestMsg, respMsgType);

   bool rrRes = requestResponse(&rrArgs);

   *outRespBuf = rrArgs.outRespBuf;
   *outRespMsg = rrArgs.outRespMsg;

   rrArgs.outRespBuf = NULL; // to avoid deletion in RequestResponseArgs destructor
   rrArgs.outRespMsg = NULL; // to avoid deletion in RequestResponseArgs destructor

   return rrRes;
}

/**
 * Sends a message to a node and receives a response.
 * Can handle target states and mapped mirror IDs. Node does not need to be referenced by caller.
 *
 * If target states are provided, communication might be skipped for certain states.
 *
 * note: rrArgs->nodeID may optionally be provided when calling this.
 * note: received message and buffer are available through rrArgs in case of success.
 */
FhgfsOpsErr MessagingTk::requestResponseNode(RequestResponseNode* rrNode,
   RequestResponseArgs* rrArgs)
{
   const char* logContext = "Messaging (RPC node)";

   NodeHandle loadedNode;

   // select the right targetID

   NumNodeID nodeID = rrNode->nodeID; // don't modify caller's nodeID

   if(rrNode->mirrorBuddies)
   { // given targetID refers to a buddy mirror group

      nodeID = rrNode->useBuddyMirrorSecond ?
         NumNodeID(rrNode->mirrorBuddies->getSecondaryTargetID(nodeID.val())) :
         NumNodeID(rrNode->mirrorBuddies->getPrimaryTargetID(nodeID.val()));

      if(unlikely(!nodeID) )
      {
         LogContext(logContext).logErr(
            "Invalid node mirror buddy group ID: " + rrNode->nodeID.str() + "; "
            "type: " + (rrArgs->node ?
               rrArgs->node->getNodeTypeStr() : rrNode->nodeStore->getStoreTypeStr() ) );

         return FhgfsOpsErr_UNKNOWNNODE;
      }
   }

   // check target state

   if(rrNode->targetStates)
   {
      CombinedTargetState targetState;
      if (!rrNode->targetStates->getState(nodeID.val(), targetState) )
      {
         LOG_DEBUG(logContext, Log_DEBUG,
            "Node has unknown state: " + nodeID.str() );
         return FhgfsOpsErr_COMMUNICATION;
      }

      rrNode->outTargetReachabilityState = targetState.reachabilityState;
      rrNode->outTargetConsistencyState = targetState.consistencyState;

      if(unlikely(
         (rrNode->outTargetReachabilityState != TargetReachabilityState_ONLINE) ||
         (rrNode->outTargetConsistencyState != TargetConsistencyState_GOOD) ) )
      {
         if(rrNode->outTargetReachabilityState == TargetReachabilityState_OFFLINE)
         {
            LOG_DEBUG(logContext, Log_SPAM,
               "Skipping communication with offline nodeID: " + nodeID.str() +"; "
               "type: " + (rrArgs->node ?
                  rrArgs->node->getNodeTypeStr() : rrNode->nodeStore->getStoreTypeStr() ) );
            return FhgfsOpsErr_COMMUNICATION; // no need to wait for offline servers
         }

         // states other than "good" and "needs resync" are not allowed with mirroring
         if(rrNode->mirrorBuddies &&
            (rrNode->outTargetConsistencyState != TargetConsistencyState_NEEDS_RESYNC) )
         {
            LOG_DEBUG(logContext, Log_DEBUG,
               "Skipping communication with mirror nodeID: " + nodeID.str() + "; "
               "node state: " +
                  TargetStateStore::stateToStr(rrNode->outTargetConsistencyState) + "; "
               "type: " + (rrArgs->node ?
                  rrArgs->node->getNodeTypeStr() : rrNode->nodeStore->getStoreTypeStr() ) );
            return FhgfsOpsErr_COMMUNICATION;
         }
      }
   }

   // reference node (if not provided by caller already)

   if(!rrArgs->node)
   {
      loadedNode = rrNode->nodeStore->referenceNode(nodeID);
      if (!loadedNode)
      {
         LogContext(logContext).log(Log_WARNING, "Unknown nodeID: " + nodeID.str() + "; "
            "type: " + rrNode->nodeStore->getStoreTypeStr());

         return FhgfsOpsErr_UNKNOWNNODE;
      }

      rrArgs->node = loadedNode.get();
   }
   else
      BEEGFS_BUG_ON_DEBUG(rrArgs->node->getNumID() != nodeID,
         "Mismatch between given rrArgs->node ID and nodeID");

   // communicate

   FhgfsOpsErr commRes = requestResponseComm(rrArgs);
   // One retry in case the connection was already broken when we got it (e.g. peer daemon restart).
   if (commRes == FhgfsOpsErr_COMMUNICATION)
   {
      LOG(COMMUNICATION, WARNING, "Retrying communication.",
            as("peer", rrNode->nodeStore->getNodeIDWithTypeStr(nodeID)),
            as("message type", rrArgs->requestMsg->getMsgTypeStr()));
      commRes = requestResponseComm(rrArgs);
   }

   if (loadedNode)
   {
      loadedNode.reset();
      rrArgs->node = nullptr;
   }

   if (commRes == FhgfsOpsErr_WOULDBLOCK)
      return FhgfsOpsErr_COMMUNICATION;
   else
      return commRes;
}

/**
 * Sends a message to the owner of a target and receives a response.
 * Can handle target states and mapped mirror IDs. Owner node of target will be resolved and
 * referenced internally.
 *
 * If target states are provided, communication might be skipped for certain states.
 *
 * note: msg header targetID is automatically set to the resolved targetID.
 *
 * @param rrArgs rrArgs->node must be NULL when calling this.
 * @return received message and buffer are available through rrArgs in case of success.
 */
FhgfsOpsErr MessagingTk::requestResponseTarget(RequestResponseTarget* rrTarget,
   RequestResponseArgs* rrArgs)
{
   const char* logContext = "Messaging (RPC target)";

   BEEGFS_BUG_ON_DEBUG(rrArgs->node, "rrArgs->node was not NULL and will leak now");

   // select the right targetID

   uint16_t targetID = rrTarget->targetID; // don't modify caller's targetID

   if(rrTarget->mirrorBuddies)
   { // given targetID refers to a buddy mirror group

      targetID = rrTarget->useBuddyMirrorSecond ?
         rrTarget->mirrorBuddies->getSecondaryTargetID(targetID) :
         rrTarget->mirrorBuddies->getPrimaryTargetID(targetID);

      if(unlikely(!targetID) )
      {
         LogContext(logContext).logErr("Invalid target mirror buddy group ID: " +
            StringTk::uintToStr(rrTarget->targetID) );

         return FhgfsOpsErr_UNKNOWNTARGET;
      }
   }

   rrArgs->requestMsg->setMsgHeaderTargetID(targetID);

   // check target state

   if(rrTarget->targetStates)
   {
      CombinedTargetState targetState;
      if (!rrTarget->targetStates->getState(targetID, targetState) )
      {
         // maybe the target was removed, check the mapper as well to be sure
         if(!rrTarget->targetMapper->targetExists(targetID) )
            return FhgfsOpsErr_UNKNOWNTARGET;

         LOG_DEBUG(logContext, Log_DEBUG,
            "Target has unknown state: " + StringTk::uintToStr(targetID) );
         return FhgfsOpsErr_COMMUNICATION;
      }

      rrTarget->outTargetReachabilityState = targetState.reachabilityState;
      rrTarget->outTargetConsistencyState = targetState.consistencyState;

      if(unlikely(
         (rrTarget->outTargetReachabilityState != TargetReachabilityState_ONLINE) ||
         (rrTarget->outTargetConsistencyState != TargetConsistencyState_GOOD) ) )
      {
         if(rrTarget->outTargetReachabilityState == TargetReachabilityState_OFFLINE)
         {
            LOG_DEBUG(logContext, Log_SPAM,
               "Skipping communication with offline targetID: " +
               StringTk::uintToStr(targetID) );
            return FhgfsOpsErr_COMMUNICATION; // no need to wait for offline servers
         }

         // states other than "good" and "resyncing" are not allowed with mirroring
         if(rrTarget->mirrorBuddies &&
            (rrTarget->outTargetConsistencyState != TargetConsistencyState_NEEDS_RESYNC) )
         {
            LOG_DEBUG(logContext, Log_DEBUG,
               "Skipping communication with mirror targetID: " +
               StringTk::uintToStr(targetID) + "; "
               "target state: " +
               TargetStateStore::stateToStr(rrTarget->outTargetConsistencyState) );
            return FhgfsOpsErr_COMMUNICATION;
         }
      }
   }

   // reference node
   auto loadedNode = rrTarget->nodeStore->referenceNodeByTargetID(
         targetID, rrTarget->targetMapper);
   if (!loadedNode)
   {
      LOG(COMMUNICATION, WARNING, "Unable to resolve storage server", targetID);
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   rrArgs->node = loadedNode.get();

   // communicate

   FhgfsOpsErr commRes = requestResponseComm(rrArgs);
   // One retry in case the connection was already broken when we got it (e.g. peer daemon restart).
   if (commRes == FhgfsOpsErr_COMMUNICATION)
   {
      LOG(COMMUNICATION, WARNING, "Retrying communication.",
            targetID, as("message type", rrArgs->requestMsg->getMsgTypeStr()));
      commRes = requestResponseComm(rrArgs);
   }

   // we ignore the _AGAIN code returned by requestResponseComm here and return it to the caller.
   // there are different reasons for why this is a good idea in deamons and in other userspace
   // tools:
   //
   // daemons are written under the assumption that no network operation can block forever. more
   // importantly, the internode syncers are written under the assumption that communication with
   // the mgmt instance does not block for very long. this assumption is very important because
   // daemons trigger state transitions (eg auto-pofflining) from the internode syncer based on
   // whether they can communicate with mgmt or not. if an internode syncer loops on _AGAIN here,
   // this handling can be delayed indefinitely. if an internodesyncer loops on _AGAIN because it is
   // trying to push an update (eg capacities) to mgmt *while mgmt is shutting down*, the
   // auto-pofflining mechanism will not fire until *after* mgmt has shut down completely. this
   // defeats the purpose of the poffline timeout wait during mgmt shutdown, which was introduced
   // to ensure that all nodes assume that all other nodes are poffline while mgmt is gone.
   //
   // userspace on the other hand should not assume that any request it sent can be transparently
   // retried. this is especially true for changes initiated by the ctl tool since these changes
   // are often based on the *current* state of the system, which may well change drastically
   // between the first request and the first retry. as such, userspace code that *knows* it can
   // retry a request should check for _AGAIN and do the retrying itself, while other parts should
   // handle _AGAIN as an error or recompute the entire request.
   if (commRes == FhgfsOpsErr_WOULDBLOCK)
      return FhgfsOpsErr_COMMUNICATION;
   else
      return commRes;
}


/**
 * Note: This version will allocate the message buffer.
 *
 * @param outBuf contains the received message; will be allocated and has to be
 * freed by the caller (only if the return value is greater than 0)
 * @return 0 on error (e.g. message to big), message length otherwise
 * @throw SocketException
 */
unsigned MessagingTk::recvMsgBuf(Socket* sock, char** outBuf, int minTimeout)
{
   const char* logContext = "MessagingTk (recv msg out-buf)";

   const int recvTimeoutMS = minTimeout < 0
      ? -1
      : std::max<int>(minTimeout, RECEIVE_TIMEOUT);

   unsigned numReceived = 0;

   *outBuf = (char*)malloc(MSGBUF_DEFAULT_SIZE);
   if(unlikely(!*outBuf) )
   {
      LogContext(logContext).log(Log_WARNING,
         "Memory allocation for incoming message buffer failed: " + sock->getPeername() );
      return 0;
   }

   try
   {
      // receive at least the message header

      numReceived += sock->recvExactT(*outBuf, NETMSG_MIN_LENGTH, 0, recvTimeoutMS);

      unsigned msgLength = NetMessageHeader::extractMsgLengthFromBuf(*outBuf, numReceived);

      if(msgLength > MSGBUF_MAX_SIZE)
      { // message too big to be accepted
         LogContext(logContext).log(Log_NOTICE,
            "Received a message with invalid length from: " + sock->getPeername() );

         SAFE_FREE(*outBuf);

         return 0;
      }
      else
      if(msgLength > MSGBUF_DEFAULT_SIZE)
      { // message larger than the default buffer
         char* oldBuf = *outBuf;

         *outBuf = (char*)realloc(*outBuf, msgLength);
         if(unlikely(!*outBuf) )
         {
            LogContext(logContext).log(Log_WARNING,
               "Memory reallocation for incoming message buffer failed: " + sock->getPeername() );
            free(oldBuf);
            return 0;
         }
      }

      // receive the rest of the message

      if(msgLength > numReceived)
         sock->recvExactT(&(*outBuf)[numReceived], msgLength-numReceived, 0, recvTimeoutMS);

      return msgLength;
   }
   catch(SocketException& e)
   {
      SAFE_FREE(*outBuf);

      throw;
   }
}

/**
 * Receive a message into a pre-alloced buffer.
 *
 * @return 0 on error (e.g. message to big), message length otherwise
 * @throw SocketException on communication error
 */
unsigned MessagingTk::recvMsgBuf(Socket* sock, char* bufIn, size_t bufInLen)
{
   const char* logContext = "MessagingTk (recv msg in-buf";

   const int recvTimeoutMS = RECEIVE_TIMEOUT;

   unsigned numReceived = 0;

   // receive at least the message header

   numReceived += sock->recvExactT(
      bufIn, NETMSG_MIN_LENGTH, 0, recvTimeoutMS);

   unsigned msgLength = NetMessageHeader::extractMsgLengthFromBuf(bufIn, numReceived);

   if(unlikely(msgLength > bufInLen) )
   { // message too big to be accepted
      LogContext(logContext).log(Log_NOTICE,
         std::string("Received a message that is too large from: ") +
         sock->getPeername() );

      return 0;
   }

   // receive the rest of the message

   if(msgLength > numReceived)
      sock->recvExactT(&bufIn[numReceived], msgLength-numReceived, 0, recvTimeoutMS);

   return msgLength;
}

/**
 * Sends a request message to a node and receives the response.
 *
 * @param rrArgs:
 *    .node receiver
 *    .requestMsg the message that should be sent to the receiver
 *    .respMsgType expected response message type
 *    .outRespBuf response buffer if successful (must be freed by the caller)
 *    .outRespMsg response message if successful (must be deleted by the caller)
 * @return FhgfsOpsErr_COMMUNICATION on comm error, FhgfsOpsErr_WOULDBLOCK if remote side
 *    encountered an indirect comm error and suggests not to try again, FhgfsOpsErr_AGAIN if other
 *    side is suggesting infinite retries.
 */
FhgfsOpsErr MessagingTk::requestResponseComm(RequestResponseArgs* rrArgs)
{
   const char* logContext = "Messaging (RPC)";

   const Node& node = *rrArgs->node;
   NodeConnPool* connPool = node.getConnPool();
   AbstractNetMessageFactory* netMessageFactory =
      PThread::getCurrentThreadApp()->getNetMessageFactory();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   // cleanup init
   Socket* sock = NULL;
   char* sendBuf = NULL;
   rrArgs->outRespBuf = NULL;
   rrArgs->outRespMsg = NULL;

   try
   {
      // connect
      sock = connPool->acquireStreamSocket();

      // prepare sendBuf
      std::pair<char*, unsigned> sendBuf = createMsgBuf(rrArgs->requestMsg);

      if(unlikely(!sendBuf.first) )
      { // malloc failed
         LogContext(logContext).logErr(
            "Memory allocation for send buffer failed. "
            "Alloc size: " + StringTk::uintToStr(sendBuf.second) );

         retVal = FhgfsOpsErr_OUTOFMEM;
         goto err_cleanup;
      }

      // send request
      sock->send(sendBuf.first, sendBuf.second, 0);
      SAFE_FREE(sendBuf.first);

      if (rrArgs->sendExtraData)
      {
         retVal = rrArgs->sendExtraData(sock, rrArgs->extraDataContext);
         if (retVal != FhgfsOpsErr_SUCCESS)
            goto err_cleanup;
      }

      // receive response
      unsigned respLength = MessagingTk::recvMsgBuf(sock, &rrArgs->outRespBuf,
            rrArgs->minTimeoutMS);
      if(unlikely(!respLength) )
      { // error (e.g. message too big)
         LogContext(logContext).log(Log_WARNING,
            "Failed to receive response from: " + node.getNodeIDWithTypeStr() + "; " +
            sock->getPeername() + ". " +
            "(Message type: " + rrArgs->requestMsg->getMsgTypeStr() + ")");

         retVal = FhgfsOpsErr_COMMUNICATION;
         goto err_cleanup;
      }

      // got response => deserialize it
      rrArgs->outRespMsg = netMessageFactory->createFromBuf(rrArgs->outRespBuf, respLength);

      if(unlikely(rrArgs->outRespMsg->getMsgType() == NETMSGTYPE_GenericResponse) )
      { // special control msg received
         retVal = handleGenericResponse(rrArgs);
         if(retVal != FhgfsOpsErr_INTERNAL)
         { // we can re-use the connection
            connPool->releaseStreamSocket(sock);
            sock = NULL;
         }

         goto err_cleanup;
      }

      if(unlikely(rrArgs->outRespMsg->getMsgType() != rrArgs->respMsgType) )
      { // response invalid (wrong msgType)
         LogContext(logContext).logErr(
            "Received invalid response type: " + rrArgs->outRespMsg->getMsgTypeStr() + "; "
            "expected: " + NetMsgStrMapping().defineToStr(rrArgs->respMsgType) + ". "
            "Disconnecting: " + node.getNodeIDWithTypeStr() + " @ " +
            sock->getPeername() );

         retVal = FhgfsOpsErr_COMMUNICATION;
         goto err_cleanup;
      }

      // got correct response

      connPool->releaseStreamSocket(sock);

      return FhgfsOpsErr_SUCCESS;
   }
   catch(SocketConnectException& e)
   {
      if ( !(rrArgs->logFlags & REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Unable to connect to: " + node.getNodeIDWithTypeStr() + ". " +
            "(Message type: " + rrArgs->requestMsg->getMsgTypeStr() + ")");
      }

      retVal = FhgfsOpsErr_COMMUNICATION;
   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr("Communication error: " + std::string(e.what() ) + "; " +
         "Peer: " + node.getNodeIDWithTypeStr() + ". "
         "(Message type: " + rrArgs->requestMsg->getMsgTypeStr() + ")");

      retVal = FhgfsOpsErr_COMMUNICATION;
   }


err_cleanup:

   // clean up...

   if(sock)
      connPool->invalidateStreamSocket(sock);

   SAFE_DELETE(rrArgs->outRespMsg);
   SAFE_FREE(rrArgs->outRespBuf);
   SAFE_FREE(sendBuf);

   return retVal;
}

/**
 * Creates a message buffer of the required size and serializes the message to it.
 *
 * @return buffer must be freed by the caller
 */
std::pair<char*, unsigned> MessagingTk::createMsgBuf(NetMessage* msg)
{
   // just bet that the message fits into an allocation of 2k. since this function is called
   // mostly with heartbeat message and ack messages, that assumption is very likely to be true.
   // other messages are not performance critical, and the overhead of a 2k allocation is small
   // enough to ignore it.
   // NULL pointers from malloc are not treated specially, because the serializers can handle
   // null pointers just fine.

   char* buf = (char*) malloc(2 * 1024);
   std::pair<bool, unsigned> serializeRes = msg->serializeMessage(buf, 2 * 1024);

   if (!serializeRes.first)
   {
      free(buf);
      buf = (char*) malloc(serializeRes.second);
      serializeRes = msg->serializeMessage(buf, serializeRes.second);
   }

   return std::make_pair(buf, serializeRes.second);
}

/**
 * Print log message and determine appropriate return code for requestResponseComm.
 *
 * If FhgfsOpsErr_INTERNAL is returned, the connection should be invalidated.
 *
 * @return FhgfsOpsErr_COMMUNICATION on indirect comm error (retry suggested),
 *    FhgfsOpsErr_WOULDBLOCK if remote side encountered an indirect comm error and suggests not to
 *    try again, FhgfsOpsErr_AGAIN if other side is suggesting infinite retries.
 */
FhgfsOpsErr MessagingTk::handleGenericResponse(RequestResponseArgs* rrArgs)
{
   const char* logContext = "Messaging (RPC)";

   FhgfsOpsErr retVal;

   GenericResponseMsg* genericResp = (GenericResponseMsg*)rrArgs->outRespMsg;

   switch(genericResp->getControlCode() )
   {
      case GenericRespMsgCode_TRYAGAIN:
      {
         if(!(rrArgs->logFlags & REQUESTRESPONSEARGS_LOGFLAG_PEERTRYAGAIN) )
         {
            rrArgs->logFlags |= REQUESTRESPONSEARGS_LOGFLAG_PEERTRYAGAIN;

            LOG(COMMUNICATION, DEBUG, "Peer is asking for a retry. Returning AGAIN to caller.",
                  as("peer", rrArgs->node->getNodeIDWithTypeStr()),
                  as("reason", genericResp->getLogStr()),
                  as("message type", rrArgs->requestMsg->getMsgTypeStr()));
         }

         retVal = FhgfsOpsErr_AGAIN;
      } break;

      case GenericRespMsgCode_INDIRECTCOMMERR:
      {
         if(!(rrArgs->logFlags & REQUESTRESPONSEARGS_LOGFLAG_PEERINDIRECTCOMMM) )
         {
            rrArgs->logFlags |= REQUESTRESPONSEARGS_LOGFLAG_PEERINDIRECTCOMMM;

            LogContext(logContext).log(Log_NOTICE,
               "Peer reported indirect communication error: " +
                  rrArgs->node->getNodeIDWithTypeStr() + "; "
               "Details: " + genericResp->getLogStr() + ". "
               "(Message type: " + rrArgs->requestMsg->getMsgTypeStr() + ")");
         }

         retVal = FhgfsOpsErr_COMMUNICATION;
      } break;

      default:
      {
         LogContext(logContext).log(Log_NOTICE,
            "Peer replied with unknown control code: " +
               rrArgs->node->getNodeIDWithTypeStr() + "; "
            "Code: " + StringTk::uintToStr(genericResp->getControlCode() ) + "; "
            "Details: " + genericResp->getLogStr() + ". "
            "(Message type: " + rrArgs->requestMsg->getMsgTypeStr() + ")");
         retVal = FhgfsOpsErr_INTERNAL;
      } break;
   }


   return retVal;
}
