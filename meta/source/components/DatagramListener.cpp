#include "DatagramListener.h"

#include <common/net/message/NetMessageTypes.h>

DatagramListener::DatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
   AcknowledgmentStore* ackStore, unsigned short udpPort, bool restrictOutboundInterfaces):
   AbstractDatagramListener("DGramLis", netFilter, localNicList, ackStore, udpPort,
      restrictOutboundInterfaces)
{
}

void DatagramListener::handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg)
{
   HighResolutionStats stats; // currently ignored
   std::shared_ptr<StandardSocket> sock = findSenderSock(fromAddr->sin_addr);
   if (sock == nullptr)
   {
      log.log(Log_WARNING, "Could not handle incoming message: no socket");
      return;
   }

   NetMessage::ResponseContext rctx(fromAddr, sock.get(), sendBuf, DGRAMMGR_SENDBUF_SIZE, &stats);

   const auto messageType = netMessageTypeToStr(msg->getMsgType());
   switch(msg->getMsgType() )
   {
      // valid messages within this context
      case NETMSGTYPE_Ack:
      case NETMSGTYPE_Dummy:
      case NETMSGTYPE_HeartbeatRequest:
      case NETMSGTYPE_Heartbeat:
      case NETMSGTYPE_MapTargets:
      case NETMSGTYPE_PublishCapacities:
      case NETMSGTYPE_RemoveNode:
      case NETMSGTYPE_RefreshCapacityPools:
      case NETMSGTYPE_RefreshStoragePools:
      case NETMSGTYPE_RefreshTargetStates:
      case NETMSGTYPE_SetMirrorBuddyGroup:
      {
         if(!msg->processIncoming(rctx) )
         {
            LOG(GENERAL, WARNING,
                  "Problem encountered during handling of incoming message.", messageType);
         }
      } break;

      default:
      { // valid, but not within this context
         log.logErr(
            "Received a message that is invalid within the current context "
            "from: " + Socket::ipaddrToStr(fromAddr->sin_addr) + "; "
            "type: " + messageType );
      } break;
   };
}


