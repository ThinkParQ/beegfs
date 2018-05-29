#ifndef SOCKET_H_
#define SOCKET_H_

#include <common/Common.h>
#include <common/system/System.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/StringTk.h>
#include <common/external/sdp_inet.h>
#include "Channel.h"
#include "NetworkInterfaceCard.h"
#include "SocketException.h"
#include "SocketConnectException.h"
#include "SocketDisconnectException.h"
#include "SocketInterruptedPollException.h"
#include "SocketTimeoutException.h"

#include <sched.h>


#define PF_SDP    AF_INET_SDP // the Sockets Direct Protocol (Family)


class Socket : public Channel
{
   public:
      virtual ~Socket();

      static std::string ipaddrToStr(struct in_addr* ipaddress);
      static std::string endpointAddrToString(struct in_addr* ipaddress, unsigned short port);
      static std::string endpointAddrToString(const char* hostname, unsigned short port);

      virtual void connect(const char* hostname, unsigned short port) = 0;
      virtual void connect(const struct sockaddr* serv_addr, socklen_t addrlen) = 0;
      virtual void bindToAddr(in_addr_t ipAddr, unsigned short port) = 0;
      virtual void listen() = 0;
      virtual Socket* accept(struct sockaddr* addr, socklen_t* addrlen) = 0;
      virtual void shutdown() = 0;
      virtual void shutdownAndRecvDisconnect(int timeoutMS) = 0;


      virtual ssize_t send(const void *buf, size_t len, int flags) = 0;
      virtual ssize_t sendto(const void *buf, size_t len, int flags,
         const struct sockaddr *to, socklen_t tolen) = 0;

      virtual ssize_t recv(void *buf, size_t len, int flags) = 0;
      virtual ssize_t recvT(void *buf, size_t len, int flags, int timeoutMS) = 0;

      void connect(const struct in_addr* ipaddress, unsigned short port);
      void bind(unsigned short port);


   protected:
      Socket();

      HighResolutionStats* stats;
      NicAddrType sockType;
      struct in_addr peerIP;
      std::string peername;


      void connect(const char* hostname, unsigned short port, int ai_family, int ai_socktype);


   private:
      static HighResolutionStats dummyStats; // used when the caller doesn't need stats


   public:
      // getters & setters
      inline uint32_t getPeerIP()
      {
         return peerIP.s_addr;
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

};

#endif /*SOCKET_H_*/
