#include <common/app/AbstractApp.h>
#include <common/system/System.h>
#include <common/threading/PThread.h>
#include <common/toolkit/StringTk.h>
#include "RDMASocketImpl.h"

#include <utility>


static RDMASocket* new_rdma_socket()
{
   return new RDMASocketImpl();
}

RDMASocket::ImplCallbacks beegfs_socket_impl = {
   IBVSocket_rdmaDevicesExist,
   IBVSocket_fork_init_once,
   new_rdma_socket,
};


// Note: Good tradeoff between throughput and mem usage (for SDR IB):
//    buf_num=64; buf_size=4*1024 (=> 512kB per socket for send and recv)

#define RDMASOCKET_DEFAULT_BUF_NUM                    (128) // moved to config
#define RDMASOCKET_DEFAULT_BUF_SIZE                   (4*1024) // moved to config
#define RDMASOCKET_DEFAULT_SL                         0;


/**
 * Note: Did you notice the rdmaForkInitOnce() method?
 *
 * @throw SocketException
 */
RDMASocketImpl::RDMASocketImpl()
{
   this->sockType = NICADDRTYPE_RDMA;

   commCfg.bufNum = RDMASOCKET_DEFAULT_BUF_NUM;
   commCfg.bufSize = RDMASOCKET_DEFAULT_BUF_SIZE;
   commCfg.serviceLevel = RDMASOCKET_DEFAULT_SL;

   this->ibvsock = IBVSocket_construct();

   if(!ibvsock)
      throw SocketException("RDMASocket allocation failed. SysErr: " + System::getErrString() );

   if(!IBVSocket_getSockValid(this->ibvsock) )
   {
      IBVSocket_destruct(this->ibvsock);
      throw SocketException("RDMASocket initialization failed. SysErr: " + System::getErrString() );
   }
}

/**
 * Note: To be used by accept() only.
 *
 * @param sock will be closed/destructed by the destructor of this object
 */
RDMASocketImpl::RDMASocketImpl(IBVSocket* ibvsock, struct in_addr peerIP, std::string peername)
{
   this->ibvsock = ibvsock;
   this->fd = IBVSocket_getRecvCompletionFD(ibvsock);

   this->peerIP = peerIP;
   this->peername = std::move(peername);

   this->sockType = NICADDRTYPE_RDMA;
}


RDMASocketImpl::~RDMASocketImpl()
{
   if(ibvsock)
      IBVSocket_destruct(ibvsock);
}

/**
 * @throw SocketException
 */
void RDMASocketImpl::connect(const char* hostname, unsigned short port)
{
   Socket::connect(hostname, port, AF_UNSPEC, SOCK_STREAM);
}

/**
 * @throw SocketException
 */
void RDMASocketImpl::connect(const struct sockaddr* serv_addr, socklen_t addrlen)
{
   unsigned short peerPort = ntohs( ( (struct sockaddr_in*)serv_addr)->sin_port );

   this->peerIP = ( (struct sockaddr_in*)serv_addr)->sin_addr;

   // set peername if not done so already (e.g. by connect(hostname) )

   if(peername.empty() )
      peername = Socket::endpointAddrToStr(peerIP, peerPort);

   bool connRes = IBVSocket_connectByIP(ibvsock, peerIP, peerPort, &commCfg);
   if(!connRes)
      throw SocketConnectException(
         std::string("RDMASocket unable to connect to: ") + std::string(peername) );


   this->fd = IBVSocket_getRecvCompletionFD(ibvsock);
}

/**
 * @throw SocketException
 */
void RDMASocketImpl::bindToAddr(in_addr_t ipAddr, unsigned short port)
{
   bool bindRes = IBVSocket_bindToAddr(ibvsock, ipAddr, port);
   if(!bindRes)
      throw SocketException("RDMASocket unable to bind to port: " +
         StringTk::uintToStr(port) );
   this->bindIP.s_addr = ipAddr;
   this->bindPort =  port;
}

/**
 * @throw SocketException
 */
void RDMASocketImpl::listen()
{
   bool listenRes = IBVSocket_listen(ibvsock);
   if(!listenRes)
      throw SocketException(std::string("RDMASocket unable to listen.") );

   this->fd = IBVSocket_getConnManagerFD(ibvsock);
   peername = std::string("Listen(Port: ") + StringTk::uintToStr(bindPort) + std::string(")");
}

/**
 * @return might return NULL in case an ignored event occurred; consider it to be a kind of false
 *    alert (=> this is not an error)
 * @throw SocketException
 */
Socket* RDMASocketImpl::accept(struct sockaddr *addr, socklen_t *addrlen)
{
   IBVSocket* acceptedIBVSocket = NULL;

   IBVSocket_AcceptRes acceptRes = IBVSocket_accept(ibvsock, &acceptedIBVSocket, addr, addrlen);
   if(acceptRes == ACCEPTRES_IGNORE)
      return NULL;
   else
   if(acceptRes == ACCEPTRES_ERR)
      throw SocketException(std::string("RDMASocket unable to accept.") );

   // prepare new socket object
   struct in_addr acceptIP = ( (struct sockaddr_in*)addr)->sin_addr;
   unsigned short acceptPort = ntohs( ( (struct sockaddr_in*)addr)->sin_port);

   std::string acceptPeername = endpointAddrToStr(acceptIP, acceptPort);

   Socket* acceptedSock = new RDMASocketImpl(acceptedIBVSocket, acceptIP, acceptPeername);

   return acceptedSock;
}

/**
 * @throw SocketException
 */
void RDMASocketImpl::shutdown()
{
   bool shutRes = IBVSocket_shutdown(ibvsock);
   if(!shutRes)
      throw SocketException(std::string("RDMASocket shutdown failed.") );
}

/**
 * Note: The RecvDisconnect-part is currently not implemented, so this is equal to the
 * normal shutdown() method.
 *
 * @throw SocketException
 */
void RDMASocketImpl::shutdownAndRecvDisconnect(int timeoutMS)
{
   this->shutdown();
}

#ifdef BEEGFS_NVFS
/**
 * Note: This is a synchronous (blocking) version
 *
 * @throw SocketException
 */
ssize_t RDMASocketImpl::read(const void *buf, size_t len, unsigned lkey, const uint64_t rbuf, unsigned rkey)
{
   size_t status = IBVSocket_read(this->ibvsock, (char *)buf, len, lkey, rbuf, rkey);
   return (status == 0) ? len : -1;
}

/**
 * Note: This is a synchronous (blocking) version
 *
 * @throw SocketException
 */
ssize_t RDMASocketImpl::write(const void *buf, size_t len, unsigned lkey, const uint64_t rbuf, unsigned rkey)
{
   size_t status = IBVSocket_write(this->ibvsock, (char *)buf, len, lkey, rbuf, rkey);
   return (status == 0) ? len : -1;
}
#endif /* BEEGFS_NVFS */

/**
 * Note: This is a synchronous (blocking) version
 *
 * @param flags ignored
 * @throw SocketException
 */
ssize_t RDMASocketImpl::send(const void *buf, size_t len, int flags)
{
   ssize_t sendRes = IBVSocket_send(ibvsock, (const char*)buf, len, flags | MSG_NOSIGNAL);
   if(sendRes == (ssize_t)len)
   {
      stats->incVals.netSendBytes += len;
      return sendRes;
   }
   else
   if(sendRes > 0)
   {
      throw SocketException(
         std::string("send(): Sent only ") + StringTk::int64ToStr(sendRes) +
         std::string(" bytes of the requested ") + StringTk::int64ToStr(len) +
         std::string(" bytes of data") );
   }

   throw SocketDisconnectException(
      "Disconnect during send() to: " + peername);
}


/**
 * Note: This is a connection-based socket type, so to and tolen are ignored.
 *
 * @param flags ignored
 * @throw SocketException
 */
ssize_t RDMASocketImpl::sendto(const void *buf, size_t len, int flags,
   const struct sockaddr *to, socklen_t tolen)
{
   ssize_t sendRes = IBVSocket_send(ibvsock, (const char*)buf, len, flags | MSG_NOSIGNAL);
   if(sendRes == (ssize_t)len)
   {
      stats->incVals.netSendBytes += len;
      return sendRes;
   }
   else
   if(sendRes > 0)
   {
      throw SocketException(
         std::string("send(): Sent only ") + StringTk::int64ToStr(sendRes) +
         std::string(" bytes of the requested ") + StringTk::int64ToStr(len) +
         std::string(" bytes of data") );
   }

   throw SocketDisconnectException(
      std::string("Disconnect during send() to: ") + peername);
}

/**
 * @param flags ignored
 * @throw SocketException
 */
ssize_t RDMASocketImpl::recv(void *buf, size_t len, int flags)
{
   ssize_t recvRes = IBVSocket_recv(ibvsock, (char*)buf, len, flags);
   if(recvRes > 0)
   {
      stats->incVals.netRecvBytes += recvRes;
      return recvRes;
   }

   if(recvRes == 0)
      throw SocketDisconnectException(std::string("Soft disconnect from ") + peername);
   else
      throw SocketDisconnectException(std::string("Recv(): Hard disconnect from ") + peername);
}


/**
 * Note: This is the default version, using poll only => see man pages of select(2) bugs section
 *
 * @param flags ignored
 * @throw SocketException
 */
ssize_t RDMASocketImpl::recvT(void *buf, size_t len, int flags, int timeoutMS)
{
   ssize_t recvRes = IBVSocket_recvT(ibvsock, (char*)buf, len, flags, timeoutMS);
   if(recvRes > 0)
   {
      stats->incVals.netRecvBytes += recvRes;
      return recvRes;
   }

   if(recvRes == -ETIMEDOUT)
      throw SocketTimeoutException("Receive timed out from: " + peername);
   else
      throw SocketDisconnectException("Received disconnect from: " + peername);
}


/**
 * Note: Don't call this for sockets that have never been connected!
 *
 * @throw SocketException
 */
void RDMASocketImpl::checkConnection()
{
   if(IBVSocket_checkConnection(ibvsock) )
      throw SocketDisconnectException("Disconnect from: " + peername);
}

/**
 * Find out whether it is possible to call recv without blocking.
 * Useful if the fd says there is incoming data (because that might be a false alarm
 * in case of an RDMASocket).
 *
 * @return 0 if no data immediately available, >0 if incoming data is available
 * @throw SocketException
 */
ssize_t RDMASocketImpl::nonblockingRecvCheck()
{
   ssize_t checkRes = IBVSocket_nonblockingRecvCheck(ibvsock);
   if(checkRes < 0)
      throw SocketDisconnectException("Disconnect from: " + peername);

   return checkRes;
}

/**
 * Call this after accept() to find out whether more events are waiting (for which
 * no notification would not be delivered through the file descriptor).
 *
 * @return true if more events are waiting and accept() should be called again
 */
bool RDMASocketImpl::checkDelayedEvents()
{
   return IBVSocket_checkDelayedEvents(ibvsock);
}
