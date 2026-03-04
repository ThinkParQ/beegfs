#include "IPAddress.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <memory>

IPAddress IPAddress::resolve(const std::string &hostname) {
   struct addrinfo hint = {};
   hint.ai_family = AF_UNSPEC;

   struct addrinfo* addressList;
   if (int err = getaddrinfo(hostname.c_str(), nullptr, &hint, &addressList))
   {
      throw std::runtime_error(std::string("Name resolution failed for ") + "'" + hostname + "': " + gai_strerror(err));
   }

   std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>
      addressListPtr(addressList, freeaddrinfo);

   addrinfo* res = nullptr;

   // Prefer ipv4
   for (auto cur = addressList; cur != nullptr; cur = cur->ai_next) {
      if(cur->ai_family == AF_INET) {
         res = cur;
         break;
      }
   }

   // No ipv4, use ipv6
   if(res == nullptr) {
      for (auto cur = addressList; cur != nullptr; cur = cur->ai_next) {
         if(cur->ai_family == AF_INET6) {
            res = cur;
            break;
         }
      }
   }

   if(res == nullptr) {
      throw std::runtime_error(std::string("Name resolution failed for ") + "'" + hostname + "': No result");
   }

   return IPAddress(res->ai_addr);
}

void IPAddress::setAddr(const sockaddr* sa) {
   this->addr = {0};

   switch (sa->sa_family) {
      case AF_INET: {
         auto s = reinterpret_cast<const sockaddr_in*>(sa);
         setAddr(s->sin_addr.s_addr);
         break;
      }
      case AF_INET6: {
         auto s = reinterpret_cast<const sockaddr_in6*>(sa);
         memcpy(this->addr.data(), &(s->sin6_addr), sizeof(this->addr));
         break;
      }
      default:
         throw std::runtime_error(std::string("Invalid address family ") + std::to_string(sa->sa_family));
   }
}

void IPAddress::setAddr(in_addr_t a4) {
   this->addr = {0};

   // Store it in network byte order as it comes in
   memcpy(&this->addr[12], &a4, 4);
   this->addr[11] = 0xff;
   this->addr[10] = 0xff;
}

void IPAddress::setAddr(const uint128_t a) {
   memcpy(this->addr.data(), &a, sizeof(a));
}

bool IPAddress::isZero() const {
   if(isIPv4()) {
      return this->addr[12] == 0 && this->addr[13] == 0 && this->addr[14] == 0 && this->addr[15] == 0;
   } else {
      return *this == IPAddress();
   }
}

bool IPAddress::isIPv4() const {
   for (int i = 0; i < 12; i++) {
      if (i < 10 && this->addr[i] != 0)
         return false;
      if (i >= 10 && this->addr[i] != 0xff)
         return false;
   }

   return true;
}

bool IPAddress::isIPv6() const {
   return !isIPv4();
}

bool IPAddress::isLoopback() const {
   if(isIPv4()) {
      return toIPv4InAddrT() == htonl(INADDR_LOOPBACK);
   } else {
      return memcmp(&addr, &in6addr_loopback, sizeof in6addr_loopback) == 0;
   }
}

bool IPAddress::isLinkLocal() const {
   if(!isIPv6())
      return false;
   
   return this->addr[0] == 0x00fe && (this->addr[1] & 0x00c0) == 0x0080;
}

uint128_t IPAddress::toUint128() const {
   uint128_t res = 0;
   // It's stored in network byte order
   memcpy(&res, this->addr.data(), sizeof(res));
   return res;
}

// Returned value is in network byte order
in_addr_t IPAddress::toIPv4InAddrT() const {
   if(isIPv4()) {
      in_addr_t res = 0;
      // It's stored in network byte order
      memcpy(&res, &this->addr[12], sizeof(res));
      return res;
   }

   throw std::runtime_error(std::string("IP address is not an IPv4 address: ") + toString());
}

in_addr IPAddress::toIPv4InAddr() const {
   in_addr res {
      .s_addr = toIPv4InAddrT()
   };

   return res;
}

SocketAddress IPAddress::toSocketAddress(uint16_t port) const {
   return SocketAddress(*this, port);
}

std::string IPAddress::toString() const {
   if (isIPv4()) {
      char buf[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &this->addr[12], buf, sizeof(buf));
      return buf;
   } else {
      char buf[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &this->addr, buf, sizeof(buf));
      return buf;
   }
}

IPAddress::operator std::string() const {
   return toString();
}

std::size_t IPAddress::hash() const {
   std::size_t hash1 = 0;
   std::size_t hash2 = 0;
   for (int i = 0; i < 16; i++) {
      if(i < 8) {
         hash1 <<= 8;
         hash1 |= this->addr[i];
      } else {
         hash2 <<= 8;
         hash2 |= this->addr[i];
      }
   }

   return hash1 ^ hash2;
}

// Helpers

// Returns port in host byte order
uint16_t extractPort(const sockaddr* a) {
   if (a->sa_family == AF_INET) {
      return ntohs(reinterpret_cast<const sockaddr_in*>(a)->sin_port);
   } else if (a->sa_family == AF_INET6) {
      return ntohs(reinterpret_cast<const sockaddr_in6*>(a)->sin6_port);
   } else {
      return 0;
   }
}

// Returns length of the specific sockaddr struct (sockaddr_in or sockaddr_in6)
size_t sockAddrLen(const sockaddr* a) {
   if (a->sa_family == AF_INET) {
      return sizeof(sockaddr_in);
   } else if (a->sa_family == AF_INET6) {
      return sizeof(sockaddr_in6);
   } else {
      return 0;
   }
}

bool IPNetwork::containsAddress(const IPAddress& a) const
{
   // Catch the special case where any ipv4 address is not included in any ipv6 network.
   // Without this, the below would match any ipv4 address in case of the network being /92 or
   // bigger.
   if (addr.isIPv6() != a.isIPv6())
      return false;

   int bytePos = 0;
   for (; bytePos < 16; bytePos++) {
      if (a.addr[bytePos] != addr.addr[bytePos]) {
         break;
      }
   }

   for (int bitPos = 7; bitPos >= 0; bitPos--) {
      if (a.addr[bytePos] >> bitPos != addr.addr[bytePos] >> bitPos) {
         // if the first different bit position (from highest to lowest) is bigger than the prefix,
         // it's the address is not in the same network
         return ((bytePos * 8) + (8 - bitPos)) > prefix;
      }
   }

   // identical addresses
   return true;
}

IPNetwork IPNetwork::fromCidr(const std::string& cidr)
{
   std::size_t s = cidr.find('/');
   if (s > 0 && s < cidr.length() - 1)
   {
      std::string addr = cidr.substr(0, s);
      uint8_t prefix = std::stoi(cidr.substr(s + 1));

      struct in6_addr ina6;
      struct in_addr ina4;
      if (::inet_pton(AF_INET6, addr.c_str(), &ina6) == 1)
      {
         if (prefix > 128)
            throw std::runtime_error(std::string("Parsing IP network CIDR '") + cidr + "' failed: Invalid IPv6 prefix " + std::to_string(prefix));
         return IPNetwork(IPAddress(ina6), prefix);
      }
      else if (::inet_pton(AF_INET, addr.c_str(), &ina4) == 1)
      {
         if (prefix > 32)
            throw std::runtime_error(std::string("Parsing IP network CIDR '") + cidr + "' failed: Invalid IPv4 prefix " + std::to_string(prefix));
         return IPNetwork(IPAddress(ina4), prefix);
      }
      else
         throw std::runtime_error(std::string("Parsing IP network CIDR '") + cidr + "' failed: Invalid address");
   }
   else
      throw std::runtime_error(std::string("Parsing IP network CIDR '") + cidr + "' failed: Invalid address");
}

SocketAddress::SocketAddress(const sockaddr* sa) {
   this->addr.setAddr(sa);

   switch (sa->sa_family) {
      case AF_INET: {
         auto s = reinterpret_cast<const sockaddr_in*>(sa);
         this->port = ntohs(s->sin_port);
         break;
      }
      case AF_INET6: {
         auto s = reinterpret_cast<const sockaddr_in6*>(sa);
         this->port = ntohs(s->sin6_port);
         break;
      }
      default:
         throw std::runtime_error(std::string("Invalid address family ") + std::to_string(sa->sa_family));
   }
}

sockaddr_in SocketAddress::toIPv4Sockaddr() const {
   sockaddr_in res {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = this->addr.toIPv4InAddr()
   };

   return res;
}

sockaddr_in6 SocketAddress::toSockaddr() const {
   sockaddr_in6 res {
      .sin6_family = AF_INET6,
      .sin6_port = htons(port),
   };

   std::memcpy(&res.sin6_addr, &this->addr.data(), sizeof(res.sin6_addr));

   return res;
}

std::string SocketAddress::toString() const {
   if(this->addr.isIPv6()) {
      return "[" + this->addr.toString() + "]:" + std::to_string(this->port);
   } else {
      return this->addr.toString() + ":" + std::to_string(this->port);
   }
}
