#pragma once

#include <common/Common.h>
#include <common/system/System.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/StringTk.h>
#include "IPAddress.h"
#include "Channel.h"
#include "NetworkInterfaceCard.h"
#include "SocketException.h"
#include "SocketConnectException.h"
#include "SocketDisconnectException.h"
#include "SocketInterruptedPollException.h"
#include "SocketTimeoutException.h"

#include <sched.h>


class Socket : public Channel
{
   public:
      virtual ~Socket();
      // port is in host order
      virtual void connect(const char* hostname, uint16_t port) = 0;
      virtual void connect(const SocketAddress& serv_addr) = 0;
      // port is in host order
      virtual void bindToAddr(const SocketAddress& ipAddr) = 0;
      virtual void listen() = 0;
      virtual Socket* accept(struct sockaddr_storage* addr, socklen_t* addrLen) = 0;
      virtual void shutdown() = 0;
      virtual void shutdownAndRecvDisconnect(int timeoutMS) = 0;

#ifdef BEEGFS_NVFS
      virtual ssize_t read(const void *buf, size_t len, uint32_t lkey, const uint64_t rbuf, uint32_t rkey) = 0;
      virtual ssize_t write(const void *buf, size_t len, uint32_t lkey, const uint64_t rbuf, uint32_t rkey) = 0;
#endif /* BEEGFS_NVFS */

      virtual ssize_t send(const void *buf, size_t len, int flags) = 0;
      virtual ssize_t sendto(const void *buf, size_t len, int flags, const SocketAddress* to) = 0;

      virtual ssize_t recv(void *buf, size_t len, int flags) = 0;
      virtual ssize_t recvT(void *buf, size_t len, int flags, int timeoutMS) = 0;

      void connect(const IPAddress& ipaddress, uint16_t port);
      void bind(uint16_t port);


   protected:
      Socket();

      HighResolutionStats* stats;
      NicAddrType sockType;
      IPAddress peerIP;
      IPAddress bindIP;
      uint16_t bindPort; // set by bindToAddr
      std::string peername;

      void connect(const char* hostname, uint16_t port, int ai_socktype);


   private:
      static HighResolutionStats dummyStats; // used when the caller doesn't need stats


   public:
      // getters & setters

      inline const IPAddress& getPeerIP() const
      {
         return peerIP;
      }

      inline const IPAddress& getBindIP() const
      {
         return bindIP;
      }

      inline std::string getPeername()
      {
         return peername;
      }

      inline NicAddrType getSockType()
      {
         return sockType;
      }

      inline void setStats(HighResolutionStats* stats)
      {
         this->stats = stats;
      }

      inline void unsetStats()
      {
         this->stats = &dummyStats;
      }

      inline uint16_t getBindPort()
      {
         return bindPort;
      }

      // inliners

      /**
       * @throw SocketException
       */
      inline ssize_t recvExact(void *buf, size_t len, int flags)
      {
         ssize_t missing = len;

         do
         {
            ssize_t recvRes = this->recv(&((char*)buf)[len-missing], missing, flags);
            missing -= recvRes;
         } while(missing);

         return (ssize_t)len;
      }

      /**
       * @throw SocketException
       */
      inline ssize_t recvExactT(void *buf, size_t len, int flags, int timeoutMS)
      {
         // note: this uses a soft timeout that is being reset after each received chunk

         ssize_t missing = len;

         do
         {
            ssize_t recvRes = recvT(&((char*)buf)[len-missing], missing, flags, timeoutMS);
            missing -= recvRes;
         } while(missing);

         return (ssize_t)len;
      }

      /**
       * @throw SocketException
       */
      inline ssize_t recvMinMax(void *buf, size_t minLen, size_t maxLen, int flags)
      {
         size_t receivedLen = 0;

         do
         {
            ssize_t recvRes = this->recv(&((char*)buf)[receivedLen], maxLen-receivedLen, flags);
            receivedLen += recvRes;
         } while(receivedLen < minLen);

         return (ssize_t)receivedLen;
      }

      /**
       * @throw SocketException
       */
      inline ssize_t recvMinMaxT(void *buf, ssize_t minLen, ssize_t maxLen, int flags,
         int timeoutMS)
      {
         // note: this uses a soft timeout that is being reset after each received chunk

         ssize_t receivedLen = 0;

         do
         {
            ssize_t recvRes =
               recvT(&((char*)buf)[receivedLen], maxLen-receivedLen, flags, timeoutMS);
            receivedLen += recvRes;
         } while(receivedLen < minLen);

         return (ssize_t)receivedLen;
      }

      static bool checkAndCacheIPv6Availability(uint16_t probePort, bool disableIPv6);
      static bool isIPv6Available();
};

