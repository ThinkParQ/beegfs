#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

#include <common/toolkit/UInt128.h>

class SocketAddress;

class IPAddress
{
   friend class IPNetwork;

   protected:
      // For simplicity, we store all ip adresses in IPv6 form as a 16 byte array, exactly
      // as it is provided by the OS. No need to deal with endianness conversion, except initially
      // when parsing an ipv4 address from in_addr_t. IPv4 addresses are stored as a mapped IPv6
      // address in the form ::ffff:a.b.c.d. 
      std::array<uint8_t, 16> addr {{0}};

   public:
      IPAddress() = default;

      explicit IPAddress(std::array<uint8_t, 16> a6) : addr(a6) {}

      explicit IPAddress(const in_addr a4) {
         setAddr(a4.s_addr);
      }
      explicit IPAddress(in_addr_t a4) {
         setAddr(a4);
      }
      void setAddr(in_addr_t a4);

      explicit IPAddress(const in6_addr a6) {
         setAddr(a6);
      }
      void setAddr(const in6_addr a6) {
         memcpy(this->addr.data(), &a6, sizeof(this->addr));
      };

      explicit IPAddress(const sockaddr* sa) {
         setAddr(sa);
      }
      void setAddr(const sockaddr* sa);

      explicit IPAddress(const sockaddr_storage* sas) {
         setAddr(sas);
      }
      void setAddr(const sockaddr_storage* sa) {
         setAddr(reinterpret_cast<const sockaddr*>(sa));
      }

      explicit IPAddress(const uint128_t a) {
         setAddr(a);
      }
      void setAddr(const uint128_t a);

      static IPAddress resolve(const std::string& hostname);

      const std::array<uint8_t, 16>& data() const {
         return this->addr;
      }

      // This does not mean "invalid" or "uninitialized" - a zero address is a valid address
      // and this type doesn't make assumptions on how it is used.
      bool isZero() const;
      bool isIPv4() const;
      bool isIPv6() const;
      bool isLoopback() const;
      bool isLinkLocal() const;

      uint128_t toUint128() const;
      in_addr_t toIPv4InAddrT() const;
      in_addr toIPv4InAddr() const;
      SocketAddress toSocketAddress(uint16_t port) const;

      std::size_t hash() const;
      std::string toString() const;
      operator std::string() const;

      bool equals(const IPAddress& o) const {
         return this->addr == o.addr;
      }

      bool operator==(const IPAddress& o) const {
         return equals(o);
      }

      bool operator!=(const IPAddress& o) const {
         return !equals(o);
      }

      bool operator<(const IPAddress& o) const {
         return addr < o.addr;
      }

      bool operator>(const IPAddress& o) const {
         return addr > o.addr;
      }
};

static inline std::ostream& operator<<(std::ostream& out, const IPAddress& ip) {
   out << ip.toString();
   return out;
}

static inline std::string operator+(const std::string& s, const IPAddress& ip) {
   return s + ip.toString();
}

namespace std
{
   // Provide hash, so this can be used as a key in maps
   template<>
   struct hash<IPAddress> {
      size_t operator() (const IPAddress& a) const {
         return a.hash();
      }
   };
};

class IPNetwork
{
   private:
      uint8_t prefix {0};
      IPAddress addr;

   public:

      IPNetwork() = default;
      IPNetwork(IPAddress address, uint8_t prefix) : addr(address) {
         if(addr.isIPv4()) {
            if(prefix > 32) {
               throw std::runtime_error(std::string("Creating IPNetwork '") + addr
                  + "/" + std::to_string(prefix) + "' failed: IPv4 prefix must be between 0 and 32");
            }
            // + 96 bits because it's mapped within an ipv6 address
            this->prefix = prefix + 96;
         } else {
            if(prefix > 128) {
               throw std::runtime_error(std::string("Creating IPNetwork '") + addr
                  + "/" + std::to_string(prefix) + "' failed: IPv6 prefix must be between 0 and 128");
            }
            this->prefix = prefix;
         }
      }

      static IPNetwork fromCidr(const std::string& cidr);
      bool containsAddress(const IPAddress& a) const;

      const IPAddress& data() const {
         return addr;
      }

      uint8_t getPrefix() const {
         return prefix;
      }

      bool isDefaultNetwork() const {
         return (addr.isIPv4() && prefix <= 96)
            || (addr.isIPv6() && prefix == 0);
      }

      bool operator==(const IPNetwork& o) const {
         return addr == o.addr && prefix == o.prefix;
      }
};

uint16_t extractPort(const sockaddr* a);
size_t sockAddrLen(const sockaddr* a);

typedef std::vector<IPNetwork> NetFilter;

class SocketAddress {
   public:
      uint16_t port;
      IPAddress addr;

      SocketAddress() = delete;
      explicit SocketAddress(const sockaddr* sa);
      explicit SocketAddress(const sockaddr_storage* sas)
         : SocketAddress(reinterpret_cast<const sockaddr*>(sas)) {}
      SocketAddress(IPAddress address, uint16_t port) : port(port), addr(address) {}

      sockaddr_in toIPv4Sockaddr() const;
      sockaddr_in6 toSockaddr() const;

      std::string toString() const;
};
