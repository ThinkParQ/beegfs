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

typedef std::unordered_map<struct in_addr, std::shared_ptr<StandardSocket>, InAddrHash> StandardSocketMap;

class AbstractDatagramListener : public PThread
{
   // for efficient individual multi-notifications (needs access to mutex)
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
      bool sendBufToNode(Node& node, const char* buf, size_t bufLen);
      bool sendMsgToNode(Node& node, NetMessage* msg);

      void sendDummyToSelfUDP();

   private:
      std::shared_ptr<StandardSocket> findSenderSockUnlocked(struct in_addr addr);

   protected:
      AbstractDatagramListener(const std::string& threadName, NetFilter* netFilter,
         NicAddressList& localNicList, AcknowledgmentStore* ackStore, unsigned short udpPort,
         bool restrictOutboundInterfaces);

      LogContext log;

      NetFilter* netFilter;
      AcknowledgmentStore* ackStore;
      bool restrictOutboundInterfaces;

      unsigned short udpPort;
      unsigned short udpPortNetByteOrder;
      in_addr_t loopbackAddrNetByteOrder; // (because INADDR_... constants are in host byte order)

      char* sendBuf;

      virtual void handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg) = 0;

      std::shared_ptr<StandardSocket> findSenderSock(struct in_addr addr);

      /**
       * Returns the mutex related to seralization of sends.
       */
      Mutex* getSendMutex()
      {
         return &mutex;
      }

   private:
      std::shared_ptr<StandardSocketGroup> udpSock;
      StandardSocketMap interfaceSocks;
      IpSourceMap ipSrcMap;
      char* recvBuf;
      int recvTimeoutMS;
      RoutingTable routingTable;
      /**
       * For now, use a single mutex for all of the members that are subject to
       * thread contention. Using multiple mutexes makes the locking more difficult
       * and can lead to deadlocks. The state dependencies are all intertwined,
       * anyway.
       */
      Mutex mutex;

      AtomicUInt32 ackCounter; // used to generate ackIDs

      NicAddressList localNicList;

      bool initSocks();
      void initBuffers();
      void configSocket(StandardSocket* sock, NicAddress* nicAddr, int bufsize);

      void run();
      void listenLoop();

      bool isDGramFromSelf(struct sockaddr_in* fromAddr);
      unsigned incAckCounter();

   public:
      // inliners

      /**
       * Returns ENETUNREACH if no local NIC found to reach @to.
       */
      ssize_t sendto(const void* buf, size_t len, int flags,
         const struct sockaddr* to, socklen_t tolen)
      {
         const std::lock_guard<Mutex> lock(mutex);
         struct in_addr a = reinterpret_cast<const struct sockaddr_in*>(to)->sin_addr;
         std::shared_ptr<StandardSocket> s = findSenderSockUnlocked(a);
         if (s == nullptr)
            return ENETUNREACH;
         return s->sendto(buf, len, flags, to, tolen);
      }

      /**
       * Returns ENETUNREACH if no local NIC found to reach @ipAddr.
       */
      ssize_t sendto(const void *buf, size_t len, int flags,
         struct in_addr ipAddr, unsigned short port)
      {
         const std::lock_guard<Mutex> lock(mutex);
         std::shared_ptr<StandardSocket> s = findSenderSockUnlocked(ipAddr);
         if (s == nullptr)
            return ENETUNREACH;
         return s->sendto(buf, len, flags, ipAddr, port);
      }

      // getters & setters
      void setRecvTimeoutMS(int recvTimeoutMS)
      {
         this->recvTimeoutMS = recvTimeoutMS;
      }

      unsigned short getUDPPort()
      {
         return udpPort;
      }

      void setLocalNicList(NicAddressList& nicList);

};

#endif /*ABSTRACTDATAGRAMLISTENER_H_*/
