#include <net/sock/ibvsocket/IBVSocket.h>
#include <common/net/sock/RDMASocket.h>

class RDMASocketImpl : public RDMASocket
{
   public:
      RDMASocketImpl();
      virtual ~RDMASocketImpl() override;

      virtual void connect(const char* hostname, unsigned short port) override;
      virtual void connect(const struct sockaddr* serv_addr, socklen_t addrlen) override;
      virtual void bindToAddr(in_addr_t ipAddr, unsigned short port) override;
      virtual void listen() override;
      virtual Socket* accept(struct sockaddr* addr, socklen_t* addrlen) override;
      virtual void shutdown() override;
      virtual void shutdownAndRecvDisconnect(int timeoutMS) override;

#ifdef BEEGFS_NVFS
      virtual ssize_t write(const void *buf, size_t len, unsigned lkey, const uint64_t rbuf, unsigned rkey) override;
      virtual ssize_t read(const void *buf, size_t len, unsigned lkey, const uint64_t rbuf, unsigned rkey) override;
#endif /* BEEGFS_NVFS */

      virtual ssize_t send(const void *buf, size_t len, int flags) override;
      virtual ssize_t sendto(const void *buf, size_t len, int flags,
         const struct sockaddr *to, socklen_t tolen) override;

      virtual ssize_t recv(void *buf, size_t len, int flags) override;
      virtual ssize_t recvT(void *buf, size_t len, int flags, int timeoutMS) override;

      virtual void checkConnection() override;
      virtual ssize_t nonblockingRecvCheck() override;
      virtual bool checkDelayedEvents() override;

   private:
      RDMASocketImpl(IBVSocket* ibvsock, struct in_addr peerIP, std::string peername);

      IBVSocket* ibvsock;
      int fd; // for pollable interface (will be cm-fd for listening sockets and recv-channel-fd
         // for connected/accepted sockets)

      IBVCommConfig commCfg;

   public:
      // getters & setters
      virtual int getFD() const override
      {
         return fd;
      }

      /**
       * Note: Only has an effect for unconnected sockets.
       */
      virtual void setBuffers(unsigned bufNum, unsigned bufSize) override
      {
         commCfg.bufNum = bufNum;
         commCfg.bufSize = bufSize;
      }

      virtual void setTimeouts(int connectMS, int flowSendMS, int pollMS)
      {
         IBVSocket_setTimeouts(ibvsock, connectMS, flowSendMS, pollMS);
      }

      /**
       * Note: Only has an effect for unconnected sockets.
       */
      virtual void setTypeOfService(uint8_t typeOfService) override
      {
         IBVSocket_setTypeOfService(ibvsock, typeOfService);
      }

     /**
      * Note: Only has an effect for testing.
      */
     virtual void setConnectionRejectionRate(unsigned rate) override
     {
        IBVSocket_setConnectionRejectionRate(ibvsock, rate);
     }
};

extern "C" {
   extern RDMASocket::ImplCallbacks beegfs_socket_impl;
}
