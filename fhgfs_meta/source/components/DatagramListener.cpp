#include "DatagramListener.h"

DatagramListener::DatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
   AcknowledgmentStore* ackStore, unsigned short udpPort)
   throw(ComponentInitException) :
   AbstractDatagramListener("DGramLis", netFilter, localNicList, ackStore, udpPort)
{
}

void DatagramListener::handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg)
{
   HighResolutionStats stats; // currently ignored
   NetMessage::ResponseContext rctx(fromAddr, udpSock, sendBuf, DGRAMMGR_SENDBUF_SIZE, &stats);

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
            log.log(Log_WARNING, "Problem encountered during handling of incoming message");
      } break;

      default:
      { // valid, but not within this context
         log.logErr(
            "Received a message that is invalid within the current context "
            "from: " + Socket::ipaddrToStr(&fromAddr->sin_addr) + "; "
            "type: " + StringTk::intToStr(msg->getMsgType() ) );
      } break;
   };
}


