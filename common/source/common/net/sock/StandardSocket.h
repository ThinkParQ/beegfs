#ifndef STANDARDSOCKET_H_
#define STANDARDSOCKET_H_

#include <common/Common.h>
#include <common/toolkit/RandomReentrant.h>
#include "PooledSocket.h"

class StandardSocket : public PooledSocket
{
   public:
      StandardSocket(int domain, int type, int protocol=0, bool epoll = true);
      virtual ~StandardSocket();

      static void createSocketPair(int domain, int type, int protocol,
         StandardSocket** outEndpointA, StandardSocket** outEndpointB);

      virtual void connect(const char* hostname, unsigned short port);
      virtual void connect(const struct sockaddr* serv_addr, socklen_t addrlen);
      virtual void bindToAddr(in_addr_t ipAddr, unsigned short port);
      virtual void listen();
      virtual Socket* accept(struct sockaddr* addr, socklen_t* addrlen);
      virtual void shutdown();
      virtual void shutdownAndRecvDisconnect(int timeoutMS);

#ifdef BEEGFS_NVFS
      virtual ssize_t read(const void *buf, size_t len, unsigned lkey, const uint64_t rbuf, unsigned rkey);
      virtual ssize_t write(const void *buf, size_t len, unsigned lkey, const uint64_t rbuf, unsigned rkey);
#endif /* BEEGFS_NVFS */

      virtual ssize_t send(const void *buf, size_t len, int flags);
      virtual ssize_t sendto(const void *buf, size_t len, int flags,
         const struct sockaddr *to, socklen_t tolen);

      virtual ssize_t recv(void *buf, size_t len, int flags);
      virtual ssize_t recvT(void *buf, size_t len, int flags, int timeoutMS);

      ssize_t recvfrom(void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
      ssize_t recvfromT(void *buf, size_t len, int flags,
         struct sockaddr *from, socklen_t *fromlen, int timeoutMS);

      ssize_t broadcast(const void *buf, size_t len, int flags,
         struct in_addr* broadcastIP, unsigned short port);


      void setSoKeepAlive(bool enable);
      void setSoBroadcast(bool enable);
      void setSoReuseAddr(bool enable);
      int getSoRcvBuf();
      void setSoRcvBuf(int size);
      void setTcpNoDelay(bool enable);
      void setTcpCork(bool enable);

   protected:
      int sock;
      unsigned short sockDomain; // socket domain (aka protocol family) e.g. PF_INET
      const bool isDgramSocket;
      int epollFD; // only valid for connected or dgram sockets, not valid (-1) for listening sockets

      StandardSocket(int fd, unsigned short sockDomain, struct in_addr peerIP,
         std::string peername);

      void addToEpoll(StandardSocket* other);

   private:
      RandomReentrant rand;

   public:
      // getters & setters
      virtual int getFD() const
      {
         return sock;
      }

      inline unsigned short getSockDomain()
      {
         return sockDomain;
      }


      // inliners

      /**
       * @throw SocketException
       */
      inline ssize_t sendto(const void *buf, size_t len, int flags,
         struct in_addr ipAddr, unsigned short port)
      {
         struct sockaddr_in peerAddr;

         // memset(&peerAddr, 0, sizeof(peerAddr) ); // not required (we set all fields below)

         peerAddr.sin_family        = sockDomain;
         peerAddr.sin_port          = htons(port);
         peerAddr.sin_addr.s_addr   = ipAddr.s_addr;

         return this->sendto(buf, len, flags,
            (struct sockaddr*)&peerAddr, sizeof(peerAddr) );
      }

      /**
       * @return true if incoming data is available, false if a timeout occurred
       * @throw SocketException on error
       */
      inline bool waitForIncomingData(int timeoutMS)
      {
            struct pollfd pollStruct = {sock, POLLIN, 0};
            int pollRes = poll(&pollStruct, 1, timeoutMS);

            if( (pollRes > 0) && (pollStruct.revents & POLLIN) )
            {
               return true;
            }
            else
            if(!pollRes)
               return false;
            else
            if(pollStruct.revents & POLLERR)
               throw SocketException(
                  std::string("waitForIncomingData(): poll(): ") + peername + ": Error condition");
            else
            if(pollStruct.revents & POLLHUP)
               throw SocketException(
                  std::string("waitForIncomingData(): poll(): ") + peername + ": Hung up");
            else
            if(pollStruct.revents & POLLNVAL)
               throw SocketException(
                  std::string("waitForIncomingData(): poll(): ") + peername +
                  ": Invalid request/fd");
            else
            if(errno == EINTR)
               throw SocketInterruptedPollException(
                  std::string("waitForIncomingData(): poll(): ") + peername + ": " +
                  System::getErrString() );
            else
               throw SocketException(
                  std::string("waitForIncomingData(): poll(): ") + peername +
                  ": " + System::getErrString() );
      }
};

/**
 * A "main" StandardSocket (this) that manages a set of subordinate StandardSocket
 * instances.
 *
 * The general idea is that "this" and subordinates can be used for sending. recvT() and
 * recfromT() are invoked upon "this" to wait for packets from any of the sockets
 * in this group.
 */
class StandardSocketGroup : public StandardSocket
{
   private:
      std::vector<std::shared_ptr<StandardSocket>> subordinates;

   public:
      StandardSocketGroup(int domain, int type, int protocol=0);

      /**
       * Create a new StandardSocket that is subordinate to "this". The socket can be used
       * for anything except methods that require epoll (i.e. recvT(), recvfromT().
       */
      std::shared_ptr<StandardSocket> createSubordinate(int domain, int type, int protocol=0);

      virtual ~StandardSocketGroup();
};
#endif /*STANDARDSOCKET_H_*/
