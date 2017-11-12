#include <common/threading/PThread.h>
#include <common/app/AbstractApp.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/TimeAbs.h>
#include <common/net/message/control/DummyMsg.h>
#include "AbstractDatagramListener.h"


AbstractDatagramListener::AbstractDatagramListener(std::string threadName, NetFilter* netFilter,
   NicAddressList& localNicList, AcknowledgmentStore* ackStore, unsigned short udpPort)
   throw(ComponentInitException) : PThread(threadName)
{
   log.setContext(threadName);

   this->recvTimeoutMS = 4000;

   this->netFilter = netFilter;
   this->localNicList = localNicList;

   this->ackStore = ackStore;

   recvBuf = NULL;
   sendBuf = NULL;

   if(!initSock(udpPort) )
      throw ComponentInitException("Unable to initialize the socket");
}

AbstractDatagramListener::~AbstractDatagramListener()
{
   delete udpSock;

   SAFE_FREE(sendBuf);
   SAFE_FREE(recvBuf);
}

bool AbstractDatagramListener::initSock(unsigned short udpPort)
{
   ICommonConfig* cfg = PThread::getCurrentThreadApp()->getCommonConfig();

   udpPortNetByteOrder = Serialization::htonsTrans(udpPort);
   loopbackAddrNetByteOrder = Serialization::htonlTrans(INADDR_LOOPBACK);

   try
   {
      udpSock = new StandardSocket(PF_INET, SOCK_DGRAM);
      udpSock->setSoBroadcast(true);
      udpSock->setSoReuseAddr(true);
      udpSock->setSoRcvBuf(cfg->getConnRDMABufNum() * cfg->getConnRDMABufSize() ); /* note:
         we're re-using rdma buffer settings here. should later be changed. */

      udpSock->bind(udpPort);

      if(udpPort == 0)
      { // find out which port we are actually using
         sockaddr_in bindAddr;
         socklen_t bindAddrLen = sizeof(bindAddr);

         int getSockNameRes = getsockname(udpSock->getFD(), (sockaddr*)&bindAddr, &bindAddrLen);
         if(getSockNameRes == -1)
            throw SocketException("getsockname() failed: " + System::getErrString() );

         udpPortNetByteOrder = bindAddr.sin_port;
         udpPort = Serialization::htonsTrans(bindAddr.sin_port);
      }

      log.log(Log_NOTICE, std::string("Listening for UDP datagrams: Port ") +
         StringTk::intToStr(udpPort) );

      return true;
   }
   catch(SocketException& e)
   {
      log.logErr(std::string("UDP socket: ") + e.what() );
      return false;
   }
}

/**
 * Note: This is for delayed buffer allocation for better NUMA performance.
 */
void AbstractDatagramListener::initBuffers()
{
   void* bufInVoid;
   void* bufOutVoid;

   int inAllocRes = posix_memalign(&bufInVoid, sysconf(_SC_PAGESIZE), DGRAMMGR_RECVBUF_SIZE);
   int outAllocRes = posix_memalign(&bufOutVoid, sysconf(_SC_PAGESIZE), DGRAMMGR_SENDBUF_SIZE);

   IGNORE_UNUSED_VARIABLE(inAllocRes);
   IGNORE_UNUSED_VARIABLE(outAllocRes);

   this->recvBuf = (char*)bufInVoid;
   this->sendBuf = (char*)bufOutVoid;
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

         AbstractNetMessageFactory* netMessageFactory =
            PThread::getCurrentThreadApp()->getNetMessageFactory();
         NetMessage* msg = netMessageFactory->createFromBuf(recvBuf, recvRes);

         handleIncomingMsg(&fromAddr, msg);

         delete(msg);

      }
      catch(SocketTimeoutException& ste)
      {
         // nothing to worry about, just idle
      }
      catch(SocketInterruptedPollException& sipe)
      {
         // ignore iterruption, because the debugger causes this
      }
      catch(SocketException& se)
      {
         log.logErr(std::string("Encountered an unrecoverable error: ") +
            se.what() );

         throw se; // to let the component exception handler be called
      }
   }

}


void AbstractDatagramListener::handleInvalidMsg(struct sockaddr_in* fromAddr)
{
   log.log(Log_NOTICE,
      "Received invalid data from: " + Socket::ipaddrToStr(&fromAddr->sin_addr) );
}

/**
 * Send to all nodes (with a static number of retries).
 *
 * @param numRetries 0 to send only once
 * @timeoutMS between retries (0 for default)
 */
void AbstractDatagramListener::sendToNodesUDP(NodeList* nodes, NetMessage* msg, int numRetries,
   int timeoutMS)
{
   int retrySleepMS = timeoutMS ? timeoutMS : 750;
   int numRetriesLeft = numRetries + 1;

   if(unlikely(nodes->empty() ) )
      return; // no nodes in the list


   while(numRetriesLeft && !getSelfTerminate() )
   {
      for(NodeListIter iter = nodes->begin(); iter != nodes->end(); iter++)
         sendMsgToNode(*iter, msg);

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
void AbstractDatagramListener::sendToNodesUDP(AbstractNodeStore* nodes, NetMessage* msg,
   int numRetries, int timeoutMS)
{
   NodeList referencedNodes;

   nodes->referenceAllNodes(&referencedNodes);

   if(referencedNodes.empty() )
      return; // no nodes in the selected store

   sendToNodesUDP(&referencedNodes, msg, numRetries, timeoutMS);

   nodes->releaseAllNodes(&referencedNodes);
}

/**
 * Send to all nodes and wait for ack.
 *
 * @return true if all acks received within a reasonable timeout.
 */
bool AbstractDatagramListener::sendToNodesUDPwithAck(NodeList* nodes, AcknowledgeableMsg* msg)
{
   int ackWaitSleepMS = 1000;
   int numRetriesLeft = 2;

   WaitAckMap waitAcks;
   WaitAckMap receivedAcks;
   WaitAckNotification notifier;

   bool allAcksReceived = false;

   // note: we use uint for tv_sec (not uint64) because 32 bits are enough here
   std::string ackIDPrefix =
      StringTk::uintToHexStr(TimeAbs().getTimeval()->tv_sec) + "-" +
      StringTk::uintToHexStr(incAckCounter() ) + "-"
      "dgramLis" "-";


   if(unlikely(nodes->empty() ) )
      return true; // no nodes in the list


   // create and register waitAcks

   for(NodeListIter iter = nodes->begin(); iter != nodes->end(); iter++)
   {
      std::string ackID(ackIDPrefix + (*iter)->getID() );
      WaitAck waitAck(ackID, *iter);

      waitAcks.insert(WaitAckMapVal(ackID, waitAck) );
   }

   ackStore->registerWaitAcks(&waitAcks, &receivedAcks, &notifier);


   // loop: send requests -> waitforcompletion -> resend

   while(numRetriesLeft && !getSelfTerminate() )
   {
      // create waitAcks copy

      SafeMutexLock waitAcksLock(&notifier.waitAcksMutex);

      WaitAckMap currentWaitAcks(waitAcks);

      waitAcksLock.unlock();

      // send messages

      for(WaitAckMapIter iter = currentWaitAcks.begin(); iter != currentWaitAcks.end(); iter++)
      {
         Node* node = (Node*)iter->second.privateData;

         msg->setAckID(iter->first.c_str() );

         sendMsgToNode(node, msg);
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

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "UDP msg ack stats: received acks: " +
      StringTk::intToStr(receivedAcks.size() ) + "/" + StringTk::intToStr(nodes->size() ) );

   return allAcksReceived;
}

/**
 * Send to all active nodes and wait for ack.
 * The message should not have an ackID set.
 *
 * @return true if all acks received within a reasonable timeout.
 */
bool AbstractDatagramListener::sendToNodesUDPwithAck(AbstractNodeStore* nodes,
   AcknowledgeableMsg* msg)
{
   NodeList referencedNodes;

   nodes->referenceAllNodes(&referencedNodes);

   if(referencedNodes.empty() )
      return true; // no nodes in the selected store

   bool sendRes = sendToNodesUDPwithAck(&referencedNodes, msg);

   nodes->releaseAllNodes(&referencedNodes);

   return sendRes;
}



/**
 * Send to node and wait for ack.
 * The message should not have an ackID set.
 *
 * @return true if ack received within a reasonable timeout.
 */
bool AbstractDatagramListener::sendToNodeUDPwithAck(Node* node, AcknowledgeableMsg* msg)
{
   NodeList nodeList;

   nodeList.push_back(node);

   bool sendRes = sendToNodesUDPwithAck(&nodeList, msg);

   return sendRes;
}


/**
 * Sends the buffer to all available node interfaces.
 */
void AbstractDatagramListener::sendBufToNode(Node* node, const char* buf, size_t bufLen)
{
   NicAddressList nicList(node->getNicList() );
   unsigned short portUDP = node->getPortUDP();

   for(NicAddressListIter iter = nicList.begin(); iter != nicList.end(); iter++)
   {
      if(iter->nicType != NICADDRTYPE_STANDARD)
         continue;

      if(!netFilter->isAllowed(iter->ipAddr.s_addr) )
         continue;

      sendto(buf, bufLen, 0, iter->ipAddr, portUDP);
   }
}

/**
 * Sends the message to all available node interfaces.
 */
void AbstractDatagramListener::sendMsgToNode(Node* node, NetMessage* msg)
{
   char* msgBuf = MessagingTk::createMsgBuf(msg);

   sendBufToNode(node, msgBuf, msg->getMsgLength() );

   free(msgBuf);
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
   char* msgBuf = MessagingTk::createMsgBuf(&msg);

   this->sendto(msgBuf, msg.getMsgLength(), 0, hostAddr,
      Serialization::ntohsTrans(udpPortNetByteOrder) );

   free(msgBuf);
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

   for(NicAddressListIter iter = localNicList.begin(); iter != localNicList.end(); iter++)
   {
      if(iter->ipAddr.s_addr == fromAddr->sin_addr.s_addr)
         return true;
   }

   if(loopbackAddrNetByteOrder == fromAddr->sin_addr.s_addr)
      return true;

   return false;
}
