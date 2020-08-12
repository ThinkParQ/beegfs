#include "DatagramListener.h"
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NetFilter.h>
#include <common/toolkit/Serialization.h>
#include <net/message/NetMessageFactory.h>
#include <linux/in.h>


void __DatagramListener_run(Thread* this)
{
   DatagramListener* thisCast = (DatagramListener*)this;

   Logger* log = App_getLogger(thisCast->app);
   const char* logContext = "DatagramListener (run)";

   __DatagramListener_initBuffers(thisCast);

   __DatagramListener_listenLoop(thisCast);

   Logger_log(log, 4, logContext, "Component stopped.");
}

void __DatagramListener_listenLoop(DatagramListener* this)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "DatagramListener (listen loop)";

   Thread* thisThread = (Thread*)this;

   fhgfs_sockaddr_in fromAddr;
   const int recvTimeoutMS = 2000;

   while(!Thread_getSelfTerminate(thisThread) )
   {
      NetMessage* msg;
      ssize_t recvRes;
      struct iovec iov = { this->recvBuf, DGRAMMGR_RECVBUF_SIZE };
      struct iov_iter iter;

      BEEGFS_IOV_ITER_INIT(&iter, READ, &iov, 1, iov.iov_len);

      recvRes = StandardSocket_recvfromT(this->udpSock, &iter, 0, &fromAddr, recvTimeoutMS);

      if(recvRes == -ETIMEDOUT)
      { // timeout: nothing to worry about, just idle
         continue;
      }
      else
      if(recvRes == 0)
      {
         char* fromIP = SocketTk_ipaddrToStr(&fromAddr.addr);
         Logger_logFormatted(log, Log_NOTICE, logContext,
            "Received an empty datagram. IP: %s; port: %d",
            fromIP, fromAddr.port);
         kfree(fromIP);

         continue;
      }
      else
      if(unlikely(recvRes < 0) )
      { // error

         Logger_logErrFormatted(log, logContext,
            "Encountered an unrecoverable socket error. ErrCode: %ld", recvRes);

         break;
      }

      if(__DatagramListener_isDGramFromLocalhost(this, &fromAddr) )
      {
         //log.log(5, "Discarding DGram from localhost");
         continue;
      }

      msg = NetMessageFactory_createFromBuf(this->app, this->recvBuf, recvRes);

      if (msg->msgHeader.msgType == NETMSGTYPE_Invalid
            || msg->msgHeader.msgLength != recvRes
            || msg->msgHeader.msgSequence != 0
            || msg->msgHeader.msgSequenceDone != 0)
      {
         char* ipStr = SocketTk_ipaddrToStr(&fromAddr.addr);

         Logger_logFormatted(this->app->logger, Log_NOTICE, logContext,
               "Received invalid message from peer %s", ipStr);
         kfree(ipStr);
      }
      else
      {
         _DatagramListener_handleIncomingMsg(this, &fromAddr, msg);
      }

      NETMESSAGE_FREE(msg);

   } // end of while loop
}

void _DatagramListener_handleIncomingMsg(DatagramListener* this,
   fhgfs_sockaddr_in* fromAddr, NetMessage* msg)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "DatagramListener (incoming msg)";

   switch(NetMessage_getMsgType(msg) )
   {
      // valid messages within this context
      case NETMSGTYPE_Ack:
      case NETMSGTYPE_HeartbeatRequest:
      case NETMSGTYPE_Heartbeat:
      case NETMSGTYPE_MapTargets:
      case NETMSGTYPE_RemoveNode:
      case NETMSGTYPE_LockGranted:
      case NETMSGTYPE_RefreshTargetStates:
      case NETMSGTYPE_SetMirrorBuddyGroup:
      {
         if(!msg->ops->processIncoming(msg, this->app, fromAddr, (Socket*)this->udpSock,
            this->sendBuf, DGRAMMGR_SENDBUF_SIZE) )
         {
            Logger_log(log, 2, logContext,
               "Problem encountered during handling of incoming message");
         }
      } break;

      default:
      { // valid fhgfs message, but not allowed within this context
         char* ipStr = SocketTk_ipaddrToStr(&fromAddr->addr);
         Logger_logErrFormatted(log, logContext, "Received a message of type %d "
            "that is invalid within the current context from: %s",
            NetMessage_getMsgType(msg), ipStr);
         kfree(ipStr);
      } break;
   };
}

bool __DatagramListener_initSock(DatagramListener* this, unsigned short udpPort)
{
   Config* cfg = App_getConfig(this->app);
   Logger* log = App_getLogger(this->app);
   const char* logContext = "DatagramListener (init sock)";

   bool broadcastRes;
   bool bindRes;
   Socket* udpSockBase;

   this->udpPortNetByteOrder = htons(udpPort);

   this->udpSock = StandardSocket_constructUDP();

   if(!this->udpSock)
   {
      Logger_logErr(log, logContext, "Initialization of UDP socket failed");
      return false;
   }

   udpSockBase = &this->udpSock->pooledSocket.socket;

   // set some socket options

   broadcastRes = StandardSocket_setSoBroadcast(this->udpSock, true);
   if(!broadcastRes)
   {
      Logger_logErr(log, logContext, "Enabling broadcast for UDP socket failed.");
      goto err_valid;
   }

   StandardSocket_setSoRcvBuf(this->udpSock,
      Config_getConnRDMABufNum(cfg) * Config_getConnRDMABufSize(cfg) );

   // bind the socket

   bindRes = Socket_bind(udpSockBase, udpPort);
   if(!bindRes)
   {
      Logger_logErrFormatted(log, logContext, "Binding UDP socket to port %d failed.", udpPort);
      goto err_valid;
   }

   Logger_logFormatted(log, 3, logContext, "Listening for UDP datagrams: Port %d", udpPort);

   return true;

err_valid:
   Socket_virtualDestruct(udpSockBase);
   return false;
}

/**
 * Note: Delayed init of buffers (better for NUMA).
 */
void __DatagramListener_initBuffers(DatagramListener* this)
{
   this->recvBuf = (char*)vmalloc(DGRAMMGR_RECVBUF_SIZE);
   this->sendBuf = (char*)vmalloc(DGRAMMGR_SENDBUF_SIZE);
}


/**
 * Sends the buffer to all available node interfaces.
 */
void DatagramListener_sendBufToNode(DatagramListener* this, Node* node,
   char* buf, size_t bufLen)
{
   NodeConnPool* connPool = Node_getConnPool(node);
   NicAddressList* nicList = NodeConnPool_getNicList(connPool);
   unsigned short port = Node_getPortUDP(node);
   NicAddressListIter iter;

   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);

      if(nicAddr->nicType != NICADDRTYPE_STANDARD)
         continue;

      if(!NetFilter_isAllowed(this->netFilter, nicAddr->ipAddr) )
         continue;

      DatagramListener_sendtoIP(this, buf, bufLen, 0, nicAddr->ipAddr, port);
   }
}

/**
 * Sends the message to all available node interfaces.
 */
void DatagramListener_sendMsgToNode(DatagramListener* this, Node* node, NetMessage* msg)
{
   char* msgBuf = MessagingTk_createMsgBuf(msg);
   unsigned msgLen = NetMessage_getMsgLength(msg);

   DatagramListener_sendBufToNode(this, node, msgBuf, msgLen);

   kfree(msgBuf);
}

bool __DatagramListener_isDGramFromLocalhost(DatagramListener* this,
   fhgfs_sockaddr_in* fromAddr)
{
   NicAddressList* nicList;
   NicAddressListIter iter;
   int nicListSize;
   int i;

   if(fromAddr->port != this->udpPortNetByteOrder)
      return false;

   nicList = Node_getNicList(this->localNode);

   NicAddressListIter_init(&iter, nicList);
   nicListSize = NicAddressList_length(nicList);

   for(i = 0; i < nicListSize; i++, NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);

      if(nicAddr->ipAddr.s_addr == fromAddr->addr.s_addr)
         return true;
   }

   if(ntohl(INADDR_LOOPBACK) == fromAddr->addr.s_addr)
      return true;

   // (inaddr_loopback is in host byte order)

   return false;
}
