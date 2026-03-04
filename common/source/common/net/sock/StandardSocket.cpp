#include <cerrno>
#include <common/app/AbstractApp.h>
#include <common/app/log/LogContext.h>
#include <common/system/System.h>
#include <common/threading/PThread.h>
#include <common/toolkit/StringTk.h>
#include "StandardSocket.h"
#include "common/net/sock/IPAddress.h"
#include "IPAddress.h"

#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>


#define STANDARDSOCKET_CONNECT_TIMEOUT_MS         5000
#define STANDARDSOCKET_UDP_COOLING_SLEEP_US      10000
#define STANDARDSOCKET_UDP_COOLING_RETRIES           5

/**
 * @throw SocketException
 */
StandardSocket::StandardSocket(int type, int protocol, bool epoll)
   : sock(-1), isDgramSocket(type == SOCK_DGRAM)
{
   if (Socket::isIPv6Available()) {
      this->sock = ::socket(AF_INET6, type, protocol);
      this->family = AF_INET6;
   } else {
      // Fall back to ipv4 if ipv6 is not supported
      this->sock = ::socket(AF_INET, type, protocol);
      this->family = AF_INET;
   }

   if(this->sock == -1) {
      throw SocketException(std::string("Error during socket creation: ") + System::getErrString());
   }

   if (epoll)
   {
      this->epollFD = epoll_create(1); // "1" is just a hint (and is actually ignored)
      if(epollFD == -1)
      {
         int sysErr = errno;
         close(sock);

         throw SocketException(std::string("Error during epoll_create(): ") +
            System::getErrString(sysErr) );
      }
      this->addToEpoll(this);
   }
   else
      this->epollFD = -1;
}

/**
 * Note: To be used by accept() or createSocketPair() only.
 *
 * @param fd will be closed by the destructor of this object
 * @throw SocketException in case epoll_create fails, the caller will need to close the
 * corresponding socket file descriptor (fd)
 */
StandardSocket::StandardSocket(int fd, const IPAddress& peerIP, std::string peername)
   : isDgramSocket(false)
{
   this->sock = fd;
   this->peerIP = peerIP;
   this->peername = peername;

   this->epollFD = epoll_create(1); // "1" is just a hint (and is actually ignored)
   if(epollFD == -1)
   {
      throw SocketException(std::string("Error during epoll_create(): ") +
         System::getErrString() );
   }

   addToEpoll(this);
}


StandardSocket::~StandardSocket()
{
   if(this->epollFD != -1)
      close(this->epollFD);

   close(this->sock);
}

/**
 * @throw SocketException
 */
void StandardSocket::createSocketPair(int type, int protocol,
   StandardSocket** outEndpointA, StandardSocket** outEndpointB)
{
   int socket_vector[2];
   in_addr loopbackv4 = {htonl(INADDR_LOOPBACK)};
   IPAddress loopbackIP(loopbackv4);

   int pairRes = socketpair(AF_UNIX, type, protocol, socket_vector);

   if(pairRes == -1)
   {
      throw SocketConnectException(
         std::string("Unable to create local SocketPair. SysErr: ") + System::getErrString() );
   }

   *outEndpointA = NULL;
   *outEndpointB = NULL;

   try
   {
      *outEndpointA = new StandardSocket(socket_vector[0], loopbackIP,
         std::string("Localhost:PeerFD#") + StringTk::intToStr(socket_vector[0]) );
      *outEndpointB = new StandardSocket(socket_vector[1], loopbackIP,
         std::string("Localhost:PeerFD#") + StringTk::intToStr(socket_vector[1]) );
   }
   catch(SocketException& e)
   {
      if(*outEndpointA)
         delete(*outEndpointA);
      else
         close(socket_vector[0]);

      if(*outEndpointB)
         delete(*outEndpointB);
      else
         close(socket_vector[1]);

      throw;
   }
}

/**
 * @throw SocketException
 */
void StandardSocket::connect(const char* hostname, uint16_t port)
{
   Socket::connect(hostname, port, SOCK_STREAM);
}

/**
 * @throw SocketException
 */
void StandardSocket::connect(const SocketAddress& serv_addr)
{
   const int timeoutMS = STANDARDSOCKET_CONNECT_TIMEOUT_MS;

   LOG(SOCKLIB, DEBUG, "Connect StandardSocket", ("socket", this), ("addr", serv_addr.toString()),
      ("bindIP", bindIP.toString()));

   // set peername if not done so already (e.g. by connect(hostname) )
   if(peername.empty() )
      peername = serv_addr.toString();

   int flagsOrig = fcntl(sock, F_GETFL, 0);
   fcntl(sock, F_SETFL, flagsOrig | O_NONBLOCK); // make the socket nonblocking

   int connRes;
   if (this->family == AF_INET) {
      // IPv4 fallback
      sockaddr_in sa = serv_addr.toIPv4Sockaddr();
      connRes = ::connect(sock, reinterpret_cast<sockaddr*>(&sa), sizeof sa);
   } else {
      sockaddr_in6 sa = serv_addr.toSockaddr();
      connRes = ::connect(sock, reinterpret_cast<sockaddr*>(&sa), sizeof sa);
   }

   if(connRes)
   {
      if(errno == EINPROGRESS)
      { // wait for "ready to send data"
         struct pollfd pollStruct = {sock, POLLOUT, 0};
         int pollRes = poll(&pollStruct, 1, timeoutMS);

         if(pollRes > 0)
         { // we got something (could also be an error)

            /* note: it's important to test ERR/HUP/NVAL here instead of POLLOUT only, because
               POLLOUT and POLLERR can be returned together. */

            if(pollStruct.revents & (POLLERR | POLLHUP | POLLNVAL) )
               throw SocketConnectException(
                  std::string("Unable to establish connection: ") + std::string(peername) );

            // connection successfully established

            fcntl(sock, F_SETFL, flagsOrig);  // set socket back to original mode
            return;
         }
         else
         if(!pollRes)
            throw SocketConnectException(
               std::string("Timeout connecting to: ") + peername );
         else
            throw SocketConnectException(
               std::string("Error connecting to: ") + peername + ". "
               "SysErr: " + System::getErrString() );

      }
   }
   else
   { // immediate connect => strange, but okay...
      fcntl(sock, F_SETFL, flagsOrig);  // set socket back to original mode
      return;
   }

   throw SocketConnectException(
      std::string("Unable to connect to: ") + peername +
      std::string(". SysErr: ") + System::getErrString() );
}

/**
 * @throw SocketException
 */
void StandardSocket::bindToAddr(const SocketAddress& addr)
{
   LOG(SOCKLIB, DEBUG, "Bind StandardSocket", ("socket", this), ("addr", addr.toString()));

   if (this->family == AF_INET) {
      // IPv4 fallback
      sockaddr_in sa;
      if (addr.addr.isZero()) {
         // Special case for addr being "::" - default bind to all addresses - this isn't valid for
         // ipv4, so we convert it here
         sa = sockaddr_in {
            .sin_port = htons(addr.port),
            .sin_addr = in_addr { htonl(INADDR_ANY) },
         };
      } else {
         sa = addr.toIPv4Sockaddr();
      }
      int bindRes = ::bind(sock, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sockaddr_in));
      if(bindRes == -1) {
         throw SocketException("Unable to bind to " + addr.toString() + ". SysErr: " + System::getErrString());
      }
   } else {
      sockaddr_in6 sa = addr.toSockaddr();
      int bindRes = ::bind(sock, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sockaddr_in6));
      if(bindRes == -1) {
         throw SocketException("Unable to bind to " + addr.toString() + ". SysErr: " + System::getErrString());
      }
   }

   bindIP = addr.addr;
   bindPort = addr.port;

   if (isDgramSocket)
      peername = std::string("Listen(Port: ") + StringTk::uintToStr(bindPort) + std::string(")");
}

/**
 * @throw SocketException
 */
void StandardSocket::listen()
{
   unsigned backlog = 16;

   if(PThread::getCurrentThreadApp() )
      backlog = PThread::getCurrentThreadApp()->getCommonConfig()->getConnBacklogTCP();

   int listenRes = ::listen(sock, backlog);
   if(listenRes == -1)
      throw SocketException(std::string("listen: ") + System::getErrString() );

   peername = std::string("Listen(Port: ") + StringTk::uintToStr(bindPort) + std::string(")");

   if(this->epollFD != -1)
   { // we won't need epoll for listening sockets (we use it only for recvT/recvfromT)
      close(this->epollFD);
      this->epollFD = -1;
   }

}

/**
 * @throw SocketException
 */
Socket* StandardSocket::accept(struct sockaddr_storage* addr, socklen_t* addrLen)
{
   int acceptRes = ::accept(sock, reinterpret_cast<struct sockaddr*>(addr), addrLen);
   if(acceptRes == -1)
   {
      throw SocketException(std::string("Error during socket accept(): ") +
         System::getErrString() );
   }

   // prepare new socket object
   uint16_t acceptPort = extractPort(reinterpret_cast<sockaddr*>(addr));
   IPAddress acceptIP(addr);

   std::string acceptPeername = acceptIP.toSocketAddress(acceptPort).toString();

   try
   {
      Socket* acceptedSock = new StandardSocket(acceptRes, acceptIP, acceptPeername);
      LOG(COMMUNICATION, DEBUG, std::string("StandardSocket::accept"),
         ("acceptPeername", acceptPeername));
      return acceptedSock;
   }
   catch(SocketException& e)
   {
      close(acceptRes);
      throw;
   }
}

/**
 * @throw SocketException
 */
void StandardSocket::shutdown()
{
   int shutRes = ::shutdown(sock, SHUT_WR);
   if(shutRes == -1)
   {
      throw SocketException(std::string("Error during socket shutdown(): ") +
         System::getErrString() );
   }
}

/**
 * @throw SocketException
 */
void StandardSocket::shutdownAndRecvDisconnect(int timeoutMS)
{
   this->shutdown();

   try
   {
      // receive until shutdown arrives
      char buf[128];
      int recvRes;
      do
      {
         recvRes = recvT(buf, sizeof(buf), 0, timeoutMS);
      } while(recvRes > 0);
   }
   catch(SocketDisconnectException& e)
   {
      // just a normal thing to happen when we shutdown :)
   }

}

#ifdef BEEGFS_NVFS
/**
 * Note: This is a synchronous (blocking) version
 *
 * @throw SocketException
 */

ssize_t StandardSocket::read(const void *buf, size_t len, uint32_t lkey, const uint64_t rbuf, uint32_t rkey)
{
      throw SocketException("Standard socket doesn't support RDMA read");
}

ssize_t StandardSocket::write(const void *buf, size_t len, uint32_t lkey, const uint64_t rbuf, uint32_t rkey)
{
      throw SocketException("Standard socket doesn't support RDMA write");
}
#endif /* BEEGFS_NVFS */

/**
 * Note: This is a synchronous (blocking) version
 *
 * @throw SocketException
 */
ssize_t StandardSocket::send(const void *buf, size_t len, int flags)
{
   ssize_t sendRes = ::send(sock, buf, len, flags | MSG_NOSIGNAL);
   if(sendRes == (ssize_t)len)
   {
      stats->incVals.netSendBytes += len;
      return sendRes;
   }
   else
   if(sendRes != -1)
   {
      throw SocketException(
         std::string("send(): Sent only ") + StringTk::int64ToStr(sendRes) +
         std::string(" bytes of the requested ") + StringTk::int64ToStr(len) +
         std::string(" bytes of data") );
   }

   throw SocketDisconnectException(
      "Disconnect during send() to: " + peername + "; "
      "SysErr: " + System::getErrString() );
}

/**
 * Note: ENETUNREACH (unreachable network) errors will be silenty discarded and not be returned to
 * the caller.
 *
 * @throw SocketException
 */
ssize_t StandardSocket::sendto(const void *buf, size_t len, int flags, const SocketAddress* to)
{
   const char* logContext = "StandardSocket::sendto";
   int tries = 0;
   int errCode = 0;
   long sleepUs = STANDARDSOCKET_UDP_COOLING_SLEEP_US;

   while (tries < STANDARDSOCKET_UDP_COOLING_RETRIES)
   {
      tries++;
      ssize_t sendRes;

      if(to == nullptr) {
         sendRes = ::sendto(sock, buf, len, flags | MSG_NOSIGNAL, nullptr, 0);
      } else if(family == AF_INET) {
         // IPv4 fallback
         sockaddr_in sa = to->toIPv4Sockaddr();
         sendRes = ::sendto(sock, buf, len, flags | MSG_NOSIGNAL, reinterpret_cast<sockaddr*>(&sa), sizeof sa);
      } else {
         sockaddr_in6 sa = to->toSockaddr();
         sendRes = ::sendto(sock, buf, len, flags | MSG_NOSIGNAL, reinterpret_cast<sockaddr*>(&sa), sizeof sa);
      }

      if(sendRes == (ssize_t)len)
      {
         stats->incVals.netSendBytes += len;
         return sendRes;
      }
      else
      if(sendRes != -1)
      {
         throw SocketException(
            std::string("send(): Sent only ") + StringTk::int64ToStr(sendRes) +
            std::string(" bytes of the requested ") + StringTk::int64ToStr(len) +
            std::string(" bytes of data") );
      }

      errCode = errno;

      if (errCode != EPERM)
         break;

      // EPERM means UDP data is being sent too fast. This implements a
      // cooling off mechanism to retry sending the data after sleeping.
      // A random value is incorporated to prevent concurrent threads from
      // waking up at the same time.
      if (tries == 1)
      {
         sleepUs += rand.getNextInRange(1, STANDARDSOCKET_UDP_COOLING_SLEEP_US);
      }
      else
      {
         sleepUs += STANDARDSOCKET_UDP_COOLING_SLEEP_US;
      }

      LogContext(logContext).log(Log_NOTICE, "Retry sendto " + peername +
         " after sleeping for " + StringTk::intToStr((int) sleepUs) +
         " us, try=" + StringTk::intToStr(tries));

      struct timespec ns = {
         .tv_sec = 0,
         .tv_nsec = sleepUs * 1000
      };
      ::nanosleep(&ns, NULL);
   }

   std::string toStr;
   if(to)
   {
      toStr = to->toString();
   }

   if(errCode == ENETUNREACH)
   {
      static bool netUnreachLogged = false; // to avoid log spamming

      if(!netUnreachLogged)
      { // log unreachable net error once
         netUnreachLogged = true;

         LogContext(logContext).log(Log_WARNING,
            "Attempted to send message to unreachable network: " + toStr + "; " +
            "peername: " + peername + "; " +
            "(This error message will be logged only once.)");
      }

      return len;
   }

   throw SocketDisconnectException("sendto(" + toStr + "): "
      "Hard Disconnect from " + peername + ": " + System::getErrString(errCode) );
}

/**
 * @throw SocketException
 */
ssize_t StandardSocket::recv(void *buf, size_t len, int flags)
{
   ssize_t recvRes = ::recv(sock, buf, len, flags);
   if(recvRes > 0)
   {
      stats->incVals.netRecvBytes += recvRes;
      return recvRes;
   }

   if(recvRes == 0)
   {
      if (isDgramSocket)
      {
         LOG(COMMUNICATION, NOTICE, "Received empty UDP datagram.", peername);
         return 0;
      }
      else
      {
         throw SocketDisconnectException(std::string("Soft disconnect from ") + peername);
      }
   }
   else
   {
      throw SocketDisconnectException(std::string("Recv(): Hard disconnect from ") +
         peername + ". SysErr: " + System::getErrString() );
   }
}

/**
 * Note: This is the default version, using epoll only => see man pages of select(2) bugs section
 *
 * @throw SocketException
 */
ssize_t StandardSocket::recvT(void *buf, size_t len, int flags, int timeoutMS)
{
   struct epoll_event epollEvent;

   if (epollFD == -1)
      throw SocketException("recvT called on non-epoll socket instance");

   while (true)
   {
      int epollRes = epoll_wait(epollFD, &epollEvent, 1, timeoutMS);

      if(likely( (epollRes > 0) && (epollEvent.events & EPOLLIN) ) )
      {
         StandardSocket* s = reinterpret_cast<StandardSocket*>(epollEvent.data.ptr);
         return s->recv(buf, len, flags);
      }
      else
      if(!epollRes)
      {
         throw SocketTimeoutException("Receive timed out from: " + peername);
         break;
      }
      else
      if(errno == EINTR)
         continue; // retry, probably interrupted by gdb
      else
      if(epollEvent.events & EPOLLHUP)
      {
         throw SocketException(
            "recvT(" + peername + "); Error: Hung up");
         break;
      }
      else
      if(epollEvent.events & EPOLLERR)
      {
         throw SocketException(
            "recvT(" + peername + "); Error condition flag set");
         break;
      }
      else
      {
         throw SocketException(
            "recvT(" + peername + "); SysErr: " + System::getErrString() );
         break;
      }
   }
}

/**
 * @throw SocketException
 */
ssize_t StandardSocket::recvfrom(void  *buf, size_t len, int flags,
   struct sockaddr_storage* from, socklen_t* fromLen)
{
   *fromLen = sizeof(*from);
   int recvRes = ::recvfrom(sock, buf, len, flags, reinterpret_cast<struct sockaddr*>(from), fromLen);
#ifdef BEEGFS_DEBUG_IP
   if (isDgramSocket)
   {
         LOG(COMMUNICATION, DEBUG, std::string("recvfrom"),
            ("addr",  (recvRes != -1 ? (from? SocketAddress(from).toString() : "null") : "error")),
            ("bindIP", bindIP.toString()),
            ("recvRes", recvRes));
   }
#endif
   if(recvRes > 0)
   {
      stats->incVals.netRecvBytes += recvRes;
      return recvRes;
   }

   if(recvRes == 0)
   {
      if (isDgramSocket)
      {
         LOG(COMMUNICATION, NOTICE, "Received empty UDP datagram.", peername,
            ("addr", (from? SocketAddress(from).toString() : std::string("null"))));
         return 0;
      }
      else
      {
         throw SocketDisconnectException(std::string("Soft disconnect from ") + peername);
      }
   }
   else
   {
      throw SocketDisconnectException(
         std::string("Recvfrom(): Hard disconnect from ") + peername + ": " +
         System::getErrString() );
   }
}

void StandardSocket::addToEpoll(StandardSocket* other)
{
   struct epoll_event epollEvent;
   epollEvent.events = EPOLLIN;
   epollEvent.data.ptr = other;

   if(epoll_ctl(epollFD, EPOLL_CTL_ADD, other->sock, &epollEvent) == -1)
   {
      int sysErr = errno;
      close(sock);
      close(epollFD);

      throw SocketException(std::string("Unable to add sock to epoll set: ") +
         System::getErrString(sysErr) );
   }

}

/**
 * This is the epoll-based version.
 *
 * @throw SocketException
 */
ssize_t StandardSocket::recvfromT(void  *buf, size_t len, int flags,
   struct sockaddr_storage *from, socklen_t* fromLen, int timeoutMS)
{
   struct epoll_event epollEvent;

   if (epollFD == -1)
      throw SocketException("recvfromT called on non-epoll socket instance");

   while (true)
   {
      int epollRes = epoll_wait(epollFD, &epollEvent, 1, timeoutMS);

      if(likely( (epollRes > 0) && (epollEvent.events & EPOLLIN) ) )
      {
         StandardSocket* s = reinterpret_cast<StandardSocket*>(epollEvent.data.ptr);
         int recvRes = s->recvfrom(buf, len, flags, from, fromLen);
         return recvRes;
      }
      else
      if(!epollRes)
      {
         throw SocketTimeoutException(std::string("Receive from ") + peername + " timed out");
         break;
      }
      else
      if(errno == EINTR)
      {
         continue; // retry, probably interrupted by gdb
      }
      else
      if(epollEvent.events & EPOLLHUP)
      {
         throw SocketException(
            std::string("recvfromT(): epoll(): ") + peername + ": Hung up");
         break;
      }
      else
      if(epollEvent.events & EPOLLERR)
      {
         throw SocketException(
            std::string("recvfromT(): epoll(): ") + peername + ": Error condition");
         break;
      }
      else
      {
         throw SocketException(
            std::string("recvfromT(): epoll(): ") + peername + ": " + System::getErrString() );
         break;
      }
   }
}

/**
 * @throw SocketException
 */
void StandardSocket::setSoKeepAlive(bool enable)
{
   int keepAliveVal = (enable ? 1 : 0);

   int setRes = setsockopt(sock,
      SOL_SOCKET,
      SO_KEEPALIVE,
      (char*)&keepAliveVal,
      sizeof(keepAliveVal) );

   if(setRes == -1)
      throw SocketException(std::string("setSoKeepAlive: ") + System::getErrString() );
}

/**
 * @throw SocketException
 */
void StandardSocket::setSoReuseAddr(bool enable)
{
   int reuseVal = (enable ? 1 : 0);

   int setRes = setsockopt(sock,
      SOL_SOCKET,
      SO_REUSEADDR,
      &reuseVal,
      sizeof(reuseVal) );

   if(setRes == -1)
      throw SocketException(std::string("setSoReuseAddr: ") + System::getErrString() );
}

/**
 * @throw SocketException
 */
int StandardSocket::getSoRcvBuf()
{
   int rcvBufLen;
   socklen_t optlen = sizeof(rcvBufLen);

   int getRes = getsockopt(sock,
      SOL_SOCKET,
      SO_RCVBUF,
      &rcvBufLen,
      &optlen);

   if(getRes == -1)
      throw SocketException(std::string("getSoRcvBuf: ") + System::getErrString() );

   return rcvBufLen;
}

/**
 * Note: Increase only (buffer will not be set to a smaller value).
 * Note: Currently, this method never throws an exception and hence doesn't return an error. (You
 *       could use getSoRcvBuf() if you're interested in the resulting buffer size.)
 *
 * @throw SocketException
 */
void StandardSocket::setSoRcvBuf(int size)
{
   /* note: according to socket(7) man page, the value given to setsockopt() is doubled and the
      doubled value is returned by getsockopt()

      update 2022-05-13: the kernel doubles the value passed to setsockopt(SO_RCVBUF) to allow
      for bookkeeping overhead. Halving the value is probably "not correct" but it's been this
      way since 2010 and changing it will potentially do more harm than good at this point.
   */

   int halfSize = size/2;
   int origBufLen = getSoRcvBuf();

   if(origBufLen >= (size) )
   { // we don't decrease buf sizes (but this is not an error)
      return;
   }

   // try without force-flag (will internally reduce to global max setting without errrors)

   int setRes = setsockopt(sock,
      SOL_SOCKET,
      SO_RCVBUF,
      &halfSize,
      sizeof(halfSize) );

   if(setRes)
      LOG_DEBUG(__func__, Log_DEBUG, std::string("setSoRcvBuf error: ") + System::getErrString() );

   // now try with force-flag (will allow root to exceed the global max setting)
   // BUT: unforunately, our suse 10 build system doesn't support the SO_RCVBUFFORCE sock opt

   /*
   setsockopt(sock,
      SOL_SOCKET,
      SO_RCVBUFFORCE,
      &halfSize,
      sizeof(halfSize) );
   */
}

/**
 * @throw SocketException
 */
void StandardSocket::setTcpNoDelay(bool enable)
{
   int noDelayVal = (enable ? 1 : 0);

   int noDelayRes = setsockopt(sock,
      IPPROTO_TCP,
      TCP_NODELAY,
      (char*)&noDelayVal,
      sizeof(noDelayVal) );

   if(noDelayRes == -1)
      throw SocketException(std::string("setTcpNoDelay: ") + System::getErrString() );
}

/**
 * @throw SocketException
 */
void StandardSocket::setTcpCork(bool enable)
{
   int corkVal = (enable ? 1 : 0);

   int setRes = setsockopt(sock,
      SOL_TCP,
      TCP_CORK,
      &corkVal,
      sizeof(corkVal) );

   if(setRes == -1)
      throw SocketException(std::string("setTcpCork: ") + System::getErrString() );
}


StandardSocketGroup::StandardSocketGroup(int type, int protocol)
   : StandardSocket(type, protocol, true)
{
}

std::shared_ptr<StandardSocket> StandardSocketGroup::createSubordinate(int type, int protocol)
{
   std::shared_ptr<StandardSocket> newSock = std::make_shared<StandardSocket>(type, protocol, false);
   subordinates.push_back(newSock);
   addToEpoll(newSock.get());
   return newSock;
}

StandardSocketGroup::~StandardSocketGroup()
{
   // close the epollFD before the subordinates get cleared to prevent potential
   // access to deleted memory in the epoll_wait handler.
   ::close(epollFD);
   epollFD = -1;
}
