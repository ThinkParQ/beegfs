#ifndef MSGHELPERACK_H_
#define MSGHELPERACK_H_

#include <common/components/AbstractDatagramListener.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/net/message/control/AckMsg.h>
#include <common/Common.h>


class MsgHelperAck
{
   public:

      
   private:
      MsgHelperAck() {}
      
      
   public:
      // inliners
      
      /**
       * Note: This will only send a response if an ackID has been set.
       * 
       * @param fromAddr the sender of the AcknowledgableMsg (so the receiver of the AckMsg)
       * @param msg the msg providing the ackID
       * @param respBuf a buffer which can be used to serialize the AckMsg
       * @return true if ackID was set
       */
      static bool respondToAckRequest(AcknowledgeableMsg* msg, struct sockaddr_in* fromAddr,
         Socket* sock, char* respBuf, size_t bufLen, AbstractDatagramListener* dgramLis)
      {
         if(!strlen(msg->getAckID() ) )
            return false;
         
         AckMsg respMsg(msg->getAckID() );
         respMsg.serialize(respBuf, bufLen);
         
         if(fromAddr)
         { // datagram => sync via dgramLis send method
            dgramLis->sendto(respBuf, respMsg.getMsgLength(), 0,
               (struct sockaddr*)fromAddr, sizeof(*fromAddr) );
         }
         else
            sock->sendto(respBuf, respMsg.getMsgLength(), 0, NULL, 0);
         
         return true;
      }
};

#endif /* MSGHELPERACK_H_ */
