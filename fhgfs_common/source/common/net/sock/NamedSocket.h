#ifndef COMMON_NET_SOCK_NAMEDSOCKET_H_
#define COMMON_NET_SOCK_NAMEDSOCKET_H_


#include "StandardSocket.h"
#include "Socket.h"

#include <sys/un.h>


class NamedSocket : public StandardSocket
{
   public:
      NamedSocket(const std::string& namedSocketPath) : StandardSocket(AF_UNIX, SOCK_STREAM, 0),
         path(namedSocketPath)
      {
         this->peername = "localhost:" + namedSocketPath;
         this->sockType = NICADDRTYPE_NAMEDSOCK;

         /**
          * Use a broadcast IP (255.255.255.255) as workaround, because the named socket doesn't
          * have an IP, so the NodeConnPoolErrorState doesn't need some special implementations for
          * the named socket
          */
         this->peerIP = {INADDR_BROADCAST};
      };


      virtual ~NamedSocket() {};

      virtual void bindToPath();
      virtual void bindToAddr(in_addr_t ipAddr, unsigned short port);

      virtual Socket* accept(struct sockaddr* addr, socklen_t* addrlen);

      virtual void connectToPath();
      virtual void connect(const char* hostname, unsigned short port);
      virtual void connect(const struct sockaddr* serv_addr, socklen_t addrlen);

   private:
      std::string path;

      /**
       * Note: To be used by accept() only.
       *
       * Note: Use a broadcast IP (255.255.255.255) as workaround, because the named socket doesn't
       * have an IP, so the NodeConnPoolErrorState doesn't need some special implementations for
       * the named socket
       *
       * @param fd will be closed by the destructor of this object
       * @throw SocketException in case epoll_create fails, the caller will need to close the
       * corresponding socket file descriptor (fd)
       */
      NamedSocket(int fd, unsigned short sockDomain, const std::string& namedSocketPath)
         : StandardSocket(fd, sockDomain, {INADDR_BROADCAST},
         "localhost:" + namedSocketPath), path(namedSocketPath)
      {
         this->sockType = NICADDRTYPE_NAMEDSOCK;
      }

   public:


};

#endif /* COMMON_NET_SOCK_NAMEDSOCKET_H_ */
