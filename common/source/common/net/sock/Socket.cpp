#include <common/app/AbstractApp.h>
#include <common/system/System.h>
#include <common/threading/PThread.h>
#include <common/toolkit/StringTk.h>
#include "Socket.h"


HighResolutionStats Socket::dummyStats; // (no need to initialize this)


/**
 * @throw SocketException
 */
Socket::Socket()
{
   this->stats = &dummyStats;

   this->sockType = NICADDRTYPE_STANDARD;

   this->peerIP.s_addr = 0;

   this->bindIP.s_addr = 0;

   this->bindPort = 0;
}

Socket::~Socket()
{
   // nothing to be done here
}


/**
 * @throw SocketException
 */
void Socket::connect(const char* hostname, unsigned short port,
   int ai_family, int ai_socktype)
{
   struct addrinfo hint;
   struct addrinfo* addressList;

   memset(&hint, 0, sizeof(struct addrinfo) );
   hint.ai_flags    = AI_CANONNAME;
   hint.ai_family   = ai_family;
   hint.ai_socktype = ai_socktype;

   int getRes = getaddrinfo(hostname, NULL, &hint, &addressList);
   if(getRes)
      throw SocketConnectException(
         std::string("Unable to resolve hostname: ") + std::string(hostname) );



   // set port and peername
   ( (struct sockaddr_in*)addressList->ai_addr)->sin_port = htons(port);

   peername = (strlen(addressList->ai_canonname) ? addressList->ai_canonname : hostname);
   peername += std::string(":") + StringTk::intToStr(port);

   try
   {
      connect(addressList->ai_addr, addressList->ai_addrlen);

      freeaddrinfo(addressList);
   }
   catch(...)
   {
      freeaddrinfo(addressList);
      throw;
   }

}

/**
 * Note: Sets the protocol family type to AF_INET.
 *
 * @throw SocketException
 */
void Socket::connect(const struct in_addr* ipaddress, unsigned short port)
{
   struct sockaddr_in serv_addr;

   memset(&serv_addr, 0, sizeof(serv_addr) );

   serv_addr.sin_family        = AF_INET;//sockDomain;
   serv_addr.sin_addr.s_addr   = ipaddress->s_addr;
   serv_addr.sin_port          = htons(port);

   this->connect( (struct sockaddr*)&serv_addr, sizeof(serv_addr) );
}

/**
 * @throw SocketException
 */
void Socket::bind(unsigned short port)
{
   in_addr_t ipAddr = INADDR_ANY;

   this->bindToAddr(ipAddr, port);
}

std::string Socket::ipaddrToStr(in_addr_t addr)
{
   char buf[4*4];
   const char* a = ::inet_ntop(AF_INET, &addr, buf, sizeof(buf));
   return a != NULL? a : "error";
}

std::string Socket::ipaddrToStr(struct in_addr ipaddress)
{
   return Socket::ipaddrToStr(ipaddress.s_addr);
}

std::string Socket::endpointAddrToStr(struct in_addr ipaddress, unsigned short port)
{
   return Socket::ipaddrToStr(ipaddress) + ":" + StringTk::uintToStr(port);
}

std::string Socket::endpointAddrToStr(in_addr_t ipaddress, unsigned short port)
{
   return Socket::ipaddrToStr(ipaddress) + ":" + StringTk::uintToStr(port);
}

std::string Socket::endpointAddrToStr(const char* hostname, unsigned short port)
{
   return std::string(hostname) + ":" + StringTk::uintToStr(port);
}

std::string Socket::endpointAddrToStr(const struct sockaddr_in* sin)
{
   return Socket::endpointAddrToStr(sin->sin_addr, ntohs(sin->sin_port));
}


