#include "RegistrationDatagramListener.h"

RegistrationDatagramListener::RegistrationDatagramListener(NetFilter* netFilter,
   NicAddressList& localNicList, AcknowledgmentStore* ackStore, unsigned short udpPort)
   throw(ComponentInitException) :
   AbstractDatagramListener("RegDGramLis", netFilter, localNicList, ackStore, udpPort)
{
}

void RegistrationDatagramListener::handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg)
{
   HighResolutionStats stats; // currently ignored
   switch(msg->getMsgType() )
   {
      // valid messages within this context
      case NETMSGTYPE_Ack:
      case NETMSGTYPE_Heartbeat:
      case NETMSGTYPE_Dummy:
      {
         if(!msg->processIncoming(fromAddr, udpSock, sendBuf, DGRAMMGR_SENDBUF_SIZE, &stats) )
            log.log(Log_WARNING, "Problem encountered during handling of incoming message");
      } break;

      case NETMSGTYPE_Invalid:
      {
         handleInvalidMsg(fromAddr);
      } break;

      default:
      { // valid, but not within this context
         log.log(Log_SPAM,
            "Received a message that is invalid within the current context "
            "from: " + Socket::ipaddrToStr(&fromAddr->sin_addr) + "; "
            "type: " + StringTk::intToStr(msg->getMsgType() ) );
      } break;
   };
}


