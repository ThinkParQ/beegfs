#include "RegistrationDatagramListener.h"

RegistrationDatagramListener::RegistrationDatagramListener(NetFilter* netFilter,
   NicAddressList& localNicList, AcknowledgmentStore* ackStore, unsigned short udpPort,
   bool restrictOutboundInterfaces) :
   AbstractDatagramListener("RegDGramLis", netFilter, localNicList, ackStore, udpPort,
      restrictOutboundInterfaces)
{
}

void RegistrationDatagramListener::handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg)
{
   HighResolutionStats stats; // currently ignored
   std::shared_ptr<StandardSocket> sock = findSenderSock(fromAddr->sin_addr);
   if (sock == nullptr)
   {
      log.log(Log_WARNING, "Could not handle incoming message: no socket");
      return;
   }

   NetMessage::ResponseContext rctx(fromAddr, sock.get(), sendBuf, DGRAMMGR_SENDBUF_SIZE, &stats);

   switch(msg->getMsgType() )
   {
      // valid messages within this context
      case NETMSGTYPE_Ack:
      case NETMSGTYPE_Heartbeat:
      case NETMSGTYPE_Dummy:
      {
         if(!msg->processIncoming(rctx) )
            log.log(Log_WARNING, "Problem encountered during handling of incoming message");
      } break;

      default:
      { // valid, but not within this context
         log.log(Log_SPAM,
            "Received a message that is invalid within the current context "
            "from: " + Socket::ipaddrToStr(fromAddr->sin_addr) + "; "
            "type: " + netMessageTypeToStr(msg->getMsgType() ) );
      } break;
   };
}


