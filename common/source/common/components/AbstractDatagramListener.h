#ifndef ABSTRACTDATAGRAMLISTENER_H_
#define ABSTRACTDATAGRAMLISTENER_H_

#include <common/app/log/LogContext.h>
#include <common/threading/Atomics.h>
#include <common/threading/PThread.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/AbstractNodeStore.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/toolkit/NetFilter.h>
#include <common/Common.h>
#include "ComponentInitException.h"

#include <mutex>

#define DGRAMMGR_RECVBUF_SIZE    65536
#define DGRAMMGR_SENDBUF_SIZE    DGRAMMGR_RECVBUF_SIZE


// forward declaration
class NetFilter;


class AbstractDatagramListener : public PThread
{
   // for efficient individual multi-notifications (needs access to sendMutex)
   friend class LockEntryNotificationWork;
   friend class LockRangeNotificationWork;


   public:
      virtual ~AbstractDatagramListener();

      void sendToNodesUDP(const AbstractNodeStore* nodes, NetMessage* msg, int numRetries,
         int timeoutMS=0);
      void sendToNodesUDP(const std::vector<NodeHandle>& nodes, NetMessage* msg, int numRetries,
         int timeoutMS=0);
      bool sendToNodesUDPwithAck(const AbstractNodeStore* nodes, AcknowledgeableMsg* msg,
         int ackWaitSleepMS = 1000, int numRetries=2);
      bool sendToNodesUDPwithAck(const std::vector<NodeHandle>& nodes, AcknowledgeableMsg* msg,
         int ackWaitSleepMS = 1000, int numRetries=2);
      bool sendToNodeUDPwithAck(const NodeHandle& node, AcknowledgeableMsg* msg,
         int ackWaitSleepMS = 1000, int numRetries=2);
      void sendBufToNode(Node& node, const char* buf, size_t bufLen);
      void sendMsgToNode(Node& node, NetMessage* msg);

      void sendDummyToSelfUDP();


   protected:
      AbstractDatagramListener(const std::string& threadName, NetFilter* netFilter,
         NicAddressList& localNicList, AcknowledgmentStore* ackStore, unsigned short udpPort);

      LogContext log;

      NetFilter* netFilter;
      AcknowledgmentStore* ackStore;
      NicAddressList localNicList;
      Mutex localNicListMutex;

      StandardSocket* udpSock;
      unsigned short udpPortNetByteOrder;
      in_addr_t loopbackAddrNetByteOrder; // (because INADDR_... constants are in host byte order)
      char* sendBuf;
      Mutex sendMutex;

      virtual void handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg) = 0;


   private:
      char* recvBuf;
      int recvTimeoutMS;

      AtomicUInt32 ackCounter; // used to generate ackIDs

      bool initSock(unsigned short udpPort);
      void initBuffers();

      void run();
      void listenLoop();

      bool isDGramFromSelf(struct sockaddr_in* fromAddr);
      unsigned incAckCounter();


   public:
      // inliners

      ssize_t sendto(const void* buf, size_t len, int flags,
         const struct sockaddr* to, socklen_t tolen)
      {
         const std::lock_guard<Mutex> lock(sendMutex);

         return udpSock->sendto(buf, len, flags, to, tolen);
      }

      ssize_t sendto(const void *buf, size_t len, int flags,
         struct in_addr ipAddr, unsigned short port)
      {
         const std::lock_guard<Mutex> lock(sendMutex);

         return udpSock->sendto(buf, len, flags, ipAddr, port);
      }

      // getters & setters
      void setRecvTimeoutMS(int recvTimeoutMS)
      {
         this->recvTimeoutMS = recvTimeoutMS;
      }

      unsigned short getUDPPort()
      {
         return ntohs(udpPortNetByteOrder);
      }

      void setLocalNicList(NicAddressList& nicList)
      {
         const std::lock_guard<Mutex> lock(localNicListMutex);
         localNicList = nicList;
      }
};

#endif /*ABSTRACTDATAGRAMLISTENER_H_*/
