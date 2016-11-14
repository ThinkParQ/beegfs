#ifndef RDMASOCKET_H_
#define RDMASOCKET_H_

#include <opentk/net/sock/ibvsocket/OpenTk_IBVSocket.h>
#include <common/Common.h>
#include "PooledSocket.h"


class RDMASocket : public PooledSocket
{
   public:
      RDMASocket();
      virtual ~RDMASocket();
      
      static bool rdmaDevicesExist();
      static void rdmaForkInitOnce();
      
      virtual void connect(const char* hostname, unsigned short port);
      virtual void connect(const struct sockaddr* serv_addr, socklen_t addrlen);
      virtual void bindToAddr(in_addr_t ipAddr, unsigned short port);
      virtual void listen();
      virtual Socket* accept(struct sockaddr* addr, socklen_t* addrlen);
      virtual void shutdown();
      virtual void shutdownAndRecvDisconnect(int timeoutMS);
      
      virtual ssize_t send(const void *buf, size_t len, int flags);
      virtual ssize_t sendto(const void *buf, size_t len, int flags,
         const struct sockaddr *to, socklen_t tolen);
      
      virtual ssize_t recv(void *buf, size_t len, int flags);
      virtual ssize_t recvT(void *buf, size_t len, int flags, int timeoutMS);

      void checkConnection();
      ssize_t nonblockingRecvCheck();
      bool checkDelayedEvents();
      
      
   protected:
      
      
   private:
      RDMASocket(IBVSocket* ibvsock, struct in_addr peerIP, std::string peername);

      IBVSocket* ibvsock;
      int fd; // for pollable interface (will be cm-fd for listening sockets and recv-channel-fd
         // for connected/accepted sockets)
      
      IBVCommConfig commCfg;
      
   public:
      // getters & setters
      virtual int getFD() const
      {
         return fd;
      }
      
      /**
       * Note: Only has an effect for unconnected sockets.
       */
      void setBuffers(unsigned bufNum, unsigned bufSize)
      {
         commCfg.bufNum = bufNum;
         commCfg.bufSize = bufSize;
      }
      
      /**
       * Note: Only has an effect for unconnected sockets.
       */
      void setTypeOfService(uint8_t typeOfService)
      {
         IBVSocket_setTypeOfService(ibvsock, typeOfService);
      }

};


#endif /*RDMASOCKET_H_*/
