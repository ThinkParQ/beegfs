#include "DatagramListener.h"

#include <common/net/message/NetMessageTypes.h>

DatagramListener::DatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
   AcknowledgmentStore* ackStore, unsigned short udpPort):
   AbstractDatagramListener("DGramLis", netFilter, localNicList, ackStore, udpPort)
{
}

void DatagramListener::handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg)
{
   HighResolutionStats stats; // currently ignored
   NetMessage::ResponseContext rctx(fromAddr, udpSock, sendBuf, DGRAMMGR_SENDBUF_SIZE, &stats);

   NetMsgStrMapping strMapping;
   const auto messageType = strMapping.defineToStr(msg->getMsgType());

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
            "from: " + Socket::ipaddrToStr(&fromAddr->sin_addr) + "; "
            "type: " + messageType );
      } break;
   };
}


