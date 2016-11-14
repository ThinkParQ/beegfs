#ifndef MESSAGINGTK_H_
#define MESSAGINGTK_H_

#include <common/net/message/NetMessage.h>
#include <common/net/sock/Socket.h>
#include <common/nodes/Node.h>
#include <common/app/log/LogContext.h>
#include <common/threading/PThread.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/app/AbstractApp.h>
#include <common/Common.h>


#define MESSAGINGTK_NUM_COMM_RETRIES   1


class MessagingTk
{
   public:
      struct RequestResponseArgs
      {
         AbstractNetMessageFactory* netMessageFactory;
         
         Node* node;
         
         NetMessage* requestMsg;
         unsigned respMsgType;
         
         char* outRespBuf;
         NetMessage* outRespMsg;
      };

   
      static unsigned recvMsgBuf(Socket* sock, char** outBuf);
      static char* createMsgBuf(NetMessage* msg);
   
   private:
      MessagingTk() {}
      
      static bool requestResponseComm(RequestResponseArgs* rrArgs);
      
   public:
      // inliners
      
      static bool requestResponse(RequestResponseArgs* rrArgs)
      {
         bool commSuccess;
         size_t numRetries = MESSAGINGTK_NUM_COMM_RETRIES;
         
         do
         {
            commSuccess = requestResponseComm(rrArgs);
            if(likely(commSuccess) )
               break;
            
         } while(numRetries--);
         
         return commSuccess;
      }

      /**
       * Sends a message and receives the response.
       *
       * @param outRespBuf the underlying buffer for outRespMsg; needs to be freed by the caller
       * if true is returned.
       * @param outRespMsg response message; needs to be freed by the caller if true is returned.
       * @return true if communication succeeded and expected response was received
       */
      static bool requestResponse(Node* node, NetMessage* requestMsg, unsigned respMsgType,
         char** outRespBuf, NetMessage** outRespMsg)
      {
         AbstractNetMessageFactory* netMessageFactory =
            PThread::getCurrentThreadApp()->getNetMessageFactory();
         
         RequestResponseArgs rrArgs;
         rrArgs.netMessageFactory = netMessageFactory;
         rrArgs.node = node;
         rrArgs.requestMsg = requestMsg;
         rrArgs.respMsgType = respMsgType;
         
         bool rrRes = requestResponse(&rrArgs);

         *outRespBuf = rrArgs.outRespBuf;
         *outRespMsg = rrArgs.outRespMsg;
         
         return rrRes;
      }
      
      /**
       * Receive a message into a pre-alloced buffer.
       * 
       * @return 0 on error (e.g. message to big), message length otherwise
       * @throw SocketException on communication error
       */ 
      static unsigned recvMsgBuf(Socket* sock, char* bufIn, size_t bufInLen)
      {
         const char* logContext = "MessagingTk (recv msg in-buf";

         const int recvTimeoutMS = CONN_LONG_TIMEOUT;
         
         unsigned numReceived = 0;
      
         // receive at least the message header
         
         numReceived += sock->recvExactT(
            bufIn, NETMSG_MIN_LENGTH, 0, recvTimeoutMS);
         
         unsigned msgLength = NetMessage::extractMsgLengthFromBuf(bufIn);
         
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
      
};

#endif /*MESSAGINGTK_H_*/
