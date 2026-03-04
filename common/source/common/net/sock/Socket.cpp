#include <atomic>
#include <cerrno>
#include <common/app/AbstractApp.h>
#include <common/system/System.h>
#include <common/threading/PThread.h>
#include <common/toolkit/StringTk.h>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <stdexcept>
#include <sys/socket.h>
#include "Socket.h"
#include "common/net/sock/IPAddress.h"


HighResolutionStats Socket::dummyStats; // (no need to initialize this)


/**
 * @throw SocketException
 */
Socket::Socket()
{
   this->stats = &dummyStats;
   this->sockType = NICADDRTYPE_STANDARD;
   this->bindPort = 0;
}

Socket::~Socket()
{
   // nothing to be done here
}


/**
 * @throw SocketException
 */
void Socket::connect(const char* hostname, uint16_t port, int ai_socktype)
{
   struct addrinfo* addressList;
   struct addrinfo* addrSelected = NULL;
   struct addrinfo hint = {};
   hint.ai_flags = AI_CANONNAME;
   hint.ai_family = AF_UNSPEC;
   hint.ai_socktype = ai_socktype;

   int getRes = getaddrinfo(hostname, NULL, &hint, &addressList);
   if(getRes)
      throw SocketConnectException(
         std::string("Unable to resolve hostname: ") + std::string(hostname) );

   for (struct addrinfo* cur = addressList; addrSelected == NULL && cur != NULL; cur = cur->ai_next)
   {
      if(cur->ai_family == AF_INET || (isIPv6Available() && cur->ai_family == AF_INET6))
         addrSelected = cur;
   }

   if (!addrSelected)
   {
      freeaddrinfo(addressList);
      throw SocketConnectException(
         std::string("No address for hostname: ") + std::string(hostname));
   }

   // set port and peername
   peername = (strlen(addrSelected->ai_canonname) ? addrSelected->ai_canonname : hostname);

   sockaddr_in6 sa6;
   if (inet_pton(AF_INET6, peername.c_str(), &sa6.sin6_addr) == 1) {
      peername = "[" + peername + "]";
   }

   peername += std::string(":") + StringTk::intToStr(port);

   freeaddrinfo(addressList);

   SocketAddress serv_addr(IPAddress(addrSelected->ai_addr), port);

   connect(serv_addr);
}


/**
 * @throw SocketException
 */
void Socket::bind(uint16_t port)
{
   this->bindToAddr(IPAddress().toSocketAddress(port));
}

static std::atomic<std::optional<bool>> ipv6Available;

bool Socket::checkAndCacheIPv6Availability(uint16_t probePort, bool disableIPv6) {
   auto cached = ipv6Available.load();
   if (cached.has_value()) {
      return cached.value();
   }

   if (disableIPv6) {
      ipv6Available.store(false);
      LOG(SOCKLIB, WARNING, "IPv6 is disabled by the configuration, falling back to IPv4 sockets");
      return false;
   }

   // Check if IPv6 socket can be created
   int sock = ::socket(AF_INET6, SOCK_STREAM, 0);

   if (sock < 0) {
      ipv6Available.store(false);
      LOG(SOCKLIB, WARNING, "IPv6 is unavailable on this host, falling back to IPv4 sockets");
      return false;
   }

   // Check if we can connect the socket to ipv6
   ::fcntl(sock, F_SETFL, O_NONBLOCK);
   auto sa = IPAddress::resolve("::1").toSocketAddress(probePort).toSockaddr();
   int res = ::connect(sock, reinterpret_cast<sockaddr*>(&sa), sizeof sa);
   if (res < 0 && errno == EADDRNOTAVAIL) {
      ::close(sock);
      ipv6Available.store(false);
      LOG(SOCKLIB, WARNING, "IPv6 is disabled on this host, falling back to IPv4 sockets");
      return false;
   }

   // Check if dual stack sockets are enabled
   int ipv6Only;
   socklen_t len = sizeof(ipv6Only);
   res = ::getsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6Only, &len);
   if (res < 0 || ipv6Only == 1) {
      ::close(sock);
      ipv6Available.store(false);
      LOG(SOCKLIB, WARNING, "IPv6 dual stack sockets are unavailable on this host, falling back to IPv4 sockets");
      return false;
   }

   ::close(sock);
   ipv6Available.store(true);
   return true;
}

bool Socket::isIPv6Available() {
   auto cached = ipv6Available.load();
   if (cached.has_value()) {
      return cached.value();
   }

   throw std::runtime_error("called isIPv6Available() without calling checkAndCacheIPv6Availability() first");
}

