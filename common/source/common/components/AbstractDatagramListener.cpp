#include <common/threading/PThread.h>
#include <common/app/AbstractApp.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/TimeAbs.h>
#include <common/net/message/control/DummyMsg.h>
#include "AbstractDatagramListener.h"

#include <mutex>

AbstractDatagramListener::AbstractDatagramListener(const std::string& threadName,
   NetFilter* netFilter, NicAddressList& localNicList, AcknowledgmentStore* ackStore,
   unsigned short udpPort, bool restrictOutboundInterfaces)
    : PThread(threadName),
      log(threadName),
      netFilter(netFilter),
      ackStore(ackStore),
      restrictOutboundInterfaces(restrictOutboundInterfaces),
      udpPort(udpPort),
      sendBuf(NULL),
      recvBuf(NULL),
      recvTimeoutMS(4000),
      localNicList(localNicList)
{
   if(!initSocks() )
      throw ComponentInitException("Unable to initialize the socket");
}

AbstractDatagramListener::~AbstractDatagramListener()
{
   SAFE_FREE(sendBuf);
   SAFE_FREE(recvBuf);
}

void AbstractDatagramListener::configSocket(StandardSocket* s, NicAddress* nicAddr, int bufsize)
{
   s->setSoBroadcast(true);
   s->setSoReuseAddr(true);
   if (bufsize > 0)
      s->setSoRcvBuf(bufsize);

   if (nicAddr)
      s->bindToAddr(nicAddr->ipAddr.s_addr, udpPort);
   else
      s->bind(udpPort);

   /*
     Note that if udpPort was intialized as zero, it will be set according to the
     value used by the first created socket and any future sockets created by this method
     will use that value. That may or may not work, depending upon the current socket
     bindings.
   */
   if(udpPort == 0)
   { // find out which port we are actually using
      sockaddr_in bindAddr;
      socklen_t bindAddrLen = sizeof(bindAddr);

      int getSockNameRes = getsockname(s->getFD(), (sockaddr*)&bindAddr, &bindAddrLen);
      if(getSockNameRes == -1)
         throw SocketException("getsockname() failed: " + System::getErrString() );

      udpPortNetByteOrder = bindAddr.sin_port;
      udpPort = htons(bindAddr.sin_port);
   }

   std::string ifname;
   if (nicAddr)
      ifname = Socket::ipaddrToStr(nicAddr->ipAddr);
   else
      ifname = "any";

   log.log(Log_NOTICE, std::string("Listening for UDP datagrams: ") + ifname + " Port " +
      StringTk::intToStr(udpPort) );

}

/**
 * mutex must be held in a multi-threaded scenario.
 */
bool AbstractDatagramListener::initSocks()
{
   auto cfg = PThread::getCurrentThreadApp()->getCommonConfig();
   int bufsize = cfg->getConnUDPRcvBufSize();

   try
   {
      udpPortNetByteOrder = htons(udpPort);
      loopbackAddrNetByteOrder = htonl(INADDR_LOOPBACK);

      if (restrictOutboundInterfaces)
      {
         routingTable = PThread::getCurrentThreadApp()->getRoutingTable();

         interfaceSocks.clear();
         udpSock = nullptr;
         ipSrcMap.clear();

         for (auto& i : localNicList)
         {
            if (i.nicType == NICADDRTYPE_STANDARD)
            {
               std::shared_ptr<StandardSocket> s;
               if (udpSock == nullptr)
               {
                  udpSock = std::make_shared<StandardSocketGroup>(PF_INET, SOCK_DGRAM);
                  s = udpSock;
               }
               else
               {
                  s = udpSock->createSubordinate(PF_INET, SOCK_DGRAM);
               }
               configSocket(s.get(), &i, bufsize);
               interfaceSocks[i.ipAddr] = s;
            }
         }
      }
      else
      {
         // no need to close down any existing unbound UDP socket, it listens to all interfaces
         if (udpSock == nullptr)
         {
            udpSock = std::make_shared<StandardSocketGroup>(PF_INET, SOCK_DGRAM);
            configSocket(udpSock.get(), NULL, bufsize);
         }
      }
   }
   catch(SocketException& e)
   {
      log.logErr(std::string("UDP socket: ") + e.what() );
      return false;
   }

   return true;
}

/**
 * Note: This is for delayed buffer allocation for better NUMA performance.
 */
void AbstractDatagramListener::initBuffers()
{
   void* bufInVoid = NULL;
   void* bufOutVoid = NULL;

   int inAllocRes = posix_memalign(&bufInVoid, sysconf(_SC_PAGESIZE), DGRAMMGR_RECVBUF_SIZE);
   int outAllocRes = posix_memalign(&bufOutVoid, sysconf(_SC_PAGESIZE), DGRAMMGR_SENDBUF_SIZE);

   IGNORE_UNUSED_VARIABLE(inAllocRes);
   IGNORE_UNUSED_VARIABLE(outAllocRes);

   this->recvBuf = (char*)bufInVoid;
   this->sendBuf = (char*)bufOutVoid;
}

std::shared_ptr<StandardSocket> AbstractDatagramListener::findSenderSock(struct in_addr addr)
{
   std::lock_guard<Mutex> lock(mutex);
   return findSenderSockUnlocked(addr);
}

/**
 * mutex must be held.
 */
std::shared_ptr<StandardSocket> AbstractDatagramListener::findSenderSockUnlocked(struct in_addr addr)
{
   std::shared_ptr<StandardSocket> sock = udpSock;
   if (restrictOutboundInterfaces)
   {
      if (addr.s_addr != loopbackAddrNetByteOrder)
      {
         struct in_addr k;
         if (auto it { ipSrcMap.find(addr) }; it != std::end(ipSrcMap))
         {
            k = it->second;
         }
         else
         {
            if (!routingTable.match(addr, localNicList, k))
            {
               k.s_addr = 0;
               // addr may have come from whoever sent a message, so this is a warning
               LOG(COMMUNICATION, WARNING, "No routes found.", ("addr", Socket::ipaddrToStr(addr)));
            }
            ipSrcMap[addr] = k;
         }

         sock = interfaceSocks[k];
         //LOG(COMMUNICATION, DEBUG, "findInterfaceSock", ("addr", Socket::ipaddrToStr(addr.s_addr).c_str()),
         //   ("k", Socket::ipaddrToStr(k).c_str()), ("sock", sock));

      }
   }

   return sock;
}

void AbstractDatagramListener::setLocalNicList(NicAddressList& nicList)
{
   const std::lock_guard<Mutex> lock(mutex);
   localNicList = nicList;
   LOG(COMMUNICATION, DEBUG, "setLocalNicList");
   initSocks();
}

void AbstractDatagramListener::run()
{
   try
   {
      registerSignalHandler();

      initBuffers();

      listenLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

void AbstractDatagramListener::listenLoop()
{
   struct sockaddr_in fromAddr;


   while(!getSelfTerminate() )
   {
      socklen_t fromAddrLen = sizeof(fromAddr);

      try
      {
         ssize_t recvRes = udpSock->recvfromT(
            recvBuf, DGRAMMGR_RECVBUF_SIZE, 0, (struct sockaddr*)&fromAddr, &fromAddrLen,
            recvTimeoutMS);
         if(isDGramFromSelf(&fromAddr) )
         { // note: important if we're sending msgs to all hosts and don't want to include ourselves
            //log.log(Log_SPAM, "Discarding DGram from localhost");
            continue;
         }

         auto netMessageFactory = PThread::getCurrentThreadApp()->getNetMessageFactory();
         auto msg = netMessageFactory->createFromRaw(recvBuf, recvRes);

         if (msg->getMsgType() == NETMSGTYPE_Invalid
               || msg->getLength() != unsigned(recvRes)
               || msg->getSequenceNumber() != 0
               || msg->getSequenceNumberDone() != 0)
         {
            LOG(COMMUNICATION, NOTICE, "Received invalid message from peer",
                  ("peer", Socket::ipaddrToStr(fromAddr.sin_addr)));
         }
         else
         {
            handleIncomingMsg(&fromAddr, msg.get());
         }
      }
      catch(SocketTimeoutException& ste)
      {
         // nothing to worry about, just idle
      }
      catch(SocketInterruptedPollException& sipe)
      {
         // ignore interruption, because the debugger causes this
      }
      catch(SocketException& se)
      {
         log.logErr(std::string("Encountered an unrecoverable error: ") +
            se.what() );

         throw se; // to let the component exception handler be called
      }
   }

}

/**
 * Send to all nodes (with a static number of retries).
 *
 * @param numRetries 0 to send only once
 * @timeoutMS between retries (0 for default)
 */
void AbstractDatagramListener::sendToNodesUDP(const std::vector<NodeHandle>& nodes, NetMessage* msg,
   int numRetries, int timeoutMS)
{
   int retrySleepMS = timeoutMS ? timeoutMS : 750;
   int numRetriesLeft = numRetries + 1;

   if (unlikely(nodes.empty()))
      return;

   while (numRetriesLeft && !getSelfTerminate())
   {
      for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
         sendMsgToNode(**iter, msg);

      numRetriesLeft--;

      if(numRetriesLeft)
         waitForSelfTerminateOrder(retrySleepMS);
   }
}


/**
 * Send to all active nodes.
 *
 * @param numRetries 0 to send only once
 * @timeoutMS between retries (0 for default)
 */
void AbstractDatagramListener::sendToNodesUDP(const AbstractNodeStore* nodes, NetMessage* msg,
   int numRetries, int timeoutMS)
{
   sendToNodesUDP(nodes->referenceAllNodes(), msg, numRetries, timeoutMS);
}

/**
 * Send to all nodes and wait for ack.
 *
 * @return true if all acks received within a timeout of ackWaitSleepMS.
 */
bool AbstractDatagramListener::sendToNodesUDPwithAck(const std::vector<NodeHandle>& nodes,
   AcknowledgeableMsg* msg, int ackWaitSleepMS, int numRetries)
{
   int numRetriesLeft = numRetries;

   WaitAckMap waitAcks;
   WaitAckMap receivedAcks;
   WaitAckNotification notifier;

   bool allAcksReceived = false;

   // note: we use uint for tv_sec (not uint64) because 32 bits are enough here
   std::string ackIDPrefix =
      StringTk::uintToHexStr(TimeAbs().getTimeval()->tv_sec) + "-" +
      StringTk::uintToHexStr(incAckCounter() ) + "-"
      "dgramLis" "-";


   if (unlikely(nodes.empty()))
      return true;


   // create and register waitAcks

   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
   {
      std::string ackID(ackIDPrefix + (*iter)->getID() );
      WaitAck waitAck(ackID, iter->get());

      waitAcks.insert(WaitAckMapVal(ackID, waitAck) );
   }

   ackStore->registerWaitAcks(&waitAcks, &receivedAcks, &notifier);


   // loop: send requests -> waitforcompletion -> resend

   while(numRetriesLeft && !getSelfTerminate() )
   {
      // create waitAcks copy

      WaitAckMap currentWaitAcks;
      {
         const std::lock_guard<Mutex> lock(notifier.waitAcksMutex);

         currentWaitAcks = waitAcks;
      }

      // send messages

      for(WaitAckMapIter iter = currentWaitAcks.begin(); iter != currentWaitAcks.end(); iter++)
      {
         Node* node = (Node*)iter->second.privateData;

         msg->setAckID(iter->first.c_str() );

         sendMsgToNode(*node, msg);
      }

      // wait for acks

      allAcksReceived = ackStore->waitForAckCompletion(&currentWaitAcks, &notifier, ackWaitSleepMS);
      if(allAcksReceived)
         break; // all acks received

      // some waitAcks left => prepare next loop

      numRetriesLeft--;
   }

   // clean up

   ackStore->unregisterWaitAcks(&waitAcks);

   LOG_DBG(COMMUNICATION, DEBUG, "UDP msg ack stats", receivedAcks.size(), nodes.size());

   return allAcksReceived;
}

/**
 * Send to all active nodes and wait for ack.
 * The message should not have an ackID set.
 *
 * @return true if all acks received within a reasonable timeout.
 */
bool AbstractDatagramListener::sendToNodesUDPwithAck(const AbstractNodeStore* nodes,
   AcknowledgeableMsg* msg, int ackWaitSleepMS, int numRetries)
{
   return sendToNodesUDPwithAck(nodes->referenceAllNodes(), msg, ackWaitSleepMS, numRetries);
}



/**
 * Send to node and wait for ack.
 * The message should not have an ackID set.
 *
 * @return true if ack received within a reasonable timeout.
 */
bool AbstractDatagramListener::sendToNodeUDPwithAck(const NodeHandle& node, AcknowledgeableMsg* msg,
   int ackWaitSleepMS, int numRetries)
{
   std::vector<NodeHandle> nodes(1, node);
   return sendToNodesUDPwithAck(nodes, msg, ackWaitSleepMS, numRetries);
}


/**
 * Sends the buffer to all available node interfaces.
 * @return true if at least one buffer was sent
 */
bool AbstractDatagramListener::sendBufToNode(Node& node, const char* buf, size_t bufLen)
{
   NicAddressList nicList(node.getNicList() );
   unsigned short portUDP = node.getPortUDP();

   bool sent = false;
   for(NicAddressListIter iter = nicList.begin(); iter != nicList.end(); iter++)
   {
      if(iter->nicType != NICADDRTYPE_STANDARD)
         continue;

      if(!netFilter->isAllowed(iter->ipAddr.s_addr) )
         continue;

      if (sendto(buf, bufLen, 0, iter->ipAddr, portUDP) >0)
         sent = true;
   }
   if (!sent)
      LOG(COMMUNICATION, ERR, std::string("Failed to send buf"), ("node", node.getNodeIDWithTypeStr()));
   return sent;
}

/**
 * Sends the message to all available node interfaces.
 */
bool AbstractDatagramListener::sendMsgToNode(Node& node, NetMessage* msg)
{
   const auto msgBuf = MessagingTk::createMsgVec(*msg);
   return sendBufToNode(node, &msgBuf[0], msgBuf.size());
}

/**
 * Note: This is intended to wake up the datagram listener thread and thus allow faster termination
 * (so you will want to call this after setting self-terminate).
 */
void AbstractDatagramListener::sendDummyToSelfUDP()
{
   in_addr hostAddr;
   hostAddr.s_addr = loopbackAddrNetByteOrder;

   DummyMsg msg;
   const auto msgBuf = MessagingTk::createMsgVec(msg);

   this->sendto(&msgBuf[0], msgBuf.size(), 0, hostAddr, udpPort);
}

/**
 * Increase by one and return previous value.
 */
unsigned AbstractDatagramListener::incAckCounter()
{
   return ackCounter.increase();
}

bool AbstractDatagramListener::isDGramFromSelf(struct sockaddr_in* fromAddr)
{
   if(fromAddr->sin_port != udpPortNetByteOrder)
      return false;

   if (loopbackAddrNetByteOrder == fromAddr->sin_addr.s_addr)
      return true;

   const std::lock_guard<Mutex> lock(mutex);
   for(NicAddressListIter iter = localNicList.begin(); iter != localNicList.end(); iter++)
   {
      //LogContext("isDGramFromSelf").log(Log_DEBUG, std::string("fromAddr=") + Socket::ipaddrToStr(&fromAddr->sin_addr)
      //+ " nic=" + Socket::ipaddrToStr(&iter->ipAddr));
      if(iter->ipAddr.s_addr == fromAddr->sin_addr.s_addr)
         return true;
   }

   return false;
}
