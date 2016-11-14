#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/net/message/NetMessageLogHelper.h>
#include <common/threading/PThread.h>
#include "MessagingTk.h"


#define MSGBUF_DEFAULT_SIZE     (64*1024) // at least big enough to store a datagram
#define MSGBUF_MAX_SIZE         (4*1024*1024) // max accepted size


/**
 * Note: This version will allocate the message buffer.
 * 
 * @param outBuf contains the received message; will be allocated and has to be
 * freed by the caller (only if the return value is greater than 0)
 * @return 0 on error (e.g. message to big), message length otherwise
 * @throw SocketException
 */ 
unsigned MessagingTk::recvMsgBuf(Socket* sock, char** outBuf)
{
   const char* logContext = "MessagingTk (recv msg out-buf)";

   const int recvTimeoutMS = CONN_LONG_TIMEOUT;
   
   unsigned numReceived = 0;

   *outBuf = (char*)malloc(MSGBUF_DEFAULT_SIZE);
   
   try
   {
      // receive at least the message header
      
      numReceived += sock->recvExactT(*outBuf, NETMSG_MIN_LENGTH, 0, recvTimeoutMS);
      
      unsigned msgLength = NetMessage::extractMsgLengthFromBuf(*outBuf);
      
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
         *outBuf = (char*)realloc(*outBuf, msgLength);
      }
      
      // receive the rest of the message

      if(msgLength > numReceived)
         sock->recvExactT(&(*outBuf)[numReceived], msgLength-numReceived, 0, recvTimeoutMS);
      
      return msgLength;
   }
   catch(SocketException& e)
   {
      SAFE_FREE(*outBuf);
      
      throw e;
   }
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
 * @return true on success, false otherwise
 */
bool MessagingTk::requestResponseComm(RequestResponseArgs* rrArgs)
{
   const char* logContext = "Messaging (RPC)";
   
   Node* node = rrArgs->node;
   NodeConnPool* connPool = node->getConnPool();

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
      size_t sendBufLen = rrArgs->requestMsg->getMsgLength();
      sendBuf = (char*)malloc(sendBufLen);

      rrArgs->requestMsg->serialize(sendBuf, sendBufLen);
   
      // send request
      sock->send(sendBuf, sendBufLen, 0);
      SAFE_FREE(sendBuf);

      // receive response
      unsigned respLength = MessagingTk::recvMsgBuf(sock, &rrArgs->outRespBuf);
      if(unlikely(!respLength) )
      { // error
         LogContext(logContext).log(Log_WARNING,
            "Failed to receive response from: " + node->getNodeIDWithTypeStr() + "; " +
            sock->getPeername() + ". " +
            "Message type: " + rrArgs->requestMsg->getMsgTypeStr() );

         goto err_cleanup;
      }

      // got response => deserialize it
      rrArgs->outRespMsg = rrArgs->netMessageFactory->createFromBuf(
         rrArgs->outRespBuf, respLength);

      if(unlikely(rrArgs->outRespMsg->getMsgType() != rrArgs->respMsgType) )
      { // response invalid (wrong msgType)
         LogContext(logContext).log(Log_WARNING,
            "Received invalid response type: " + rrArgs->outRespMsg->getMsgTypeStr() + "; "
            "expected: " + NetMsgStrMapping().defineToStr(rrArgs->respMsgType) + ". "
            "Disconnecting: " + node->getNodeIDWithTypeStr() + "; " +
            sock->getPeername() );
         
         goto err_cleanup;
      }

      // got correct response

      connPool->releaseStreamSocket(sock);

      return true;
   }
   catch(SocketConnectException& e)
   {
      LogContext(logContext).log(Log_WARNING,
         "Unable to connect to: " + node->getNodeIDWithTypeStr() + ". " +
         "Message type: " + rrArgs->requestMsg->getMsgTypeStr() );
   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr("Communication error: " + std::string(e.what() ) + "; " +
         "Peer: " + node->getNodeIDWithTypeStr() + ". "
         "Message type: " + rrArgs->requestMsg->getMsgTypeStr() );
   }

   
err_cleanup:

   // clean up...

   if(sock)
      connPool->invalidateStreamSocket(sock);

   SAFE_DELETE(rrArgs->outRespMsg);
   SAFE_FREE(rrArgs->outRespBuf);
   SAFE_FREE(sendBuf);

   return false;
}

/**
 * Creates a message buffer of the required size and serializes the message to it.
 * 
 * @return buffer must be freed by the caller
 */
char* MessagingTk::createMsgBuf(NetMessage* msg)
{
   size_t bufLen = msg->getMsgLength();
   char* buf = (char*)malloc(bufLen);
   
   msg->serialize(buf, bufLen);
   
   return buf;
}









