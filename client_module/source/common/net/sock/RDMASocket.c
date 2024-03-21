#include <common/net/sock/RDMASocket.h>
#include <common/Common.h>

#include <linux/in.h>
#include <linux/poll.h>


// Note: These are historical defaults designed for SDR IB and do not provide
//    the best performance for current IB fabrics. Ideally, buf_size should be
//    configured as the largest chunksize used by the filesystem and buf_num
//    will be 3. It would be ideal to take buf_num down to 1, but the current
//    protocol requires at least 3 buffers.
//    buf_num=64; buf_size=4*1024 (=> 512kB per socket for send and recv)

#define RDMASOCKET_DEFAULT_BUF_NUM                    (128) // moved to config
#define RDMASOCKET_DEFAULT_BUF_SIZE                   (4*1024) // moved to config
#define RDMASOCKET_DEFAULT_FRAGMENT_SIZE              RDMASOCKET_DEFAULT_BUF_SIZE // moved to config
#define RDMASOCKET_DEFAULT_KEY_TYPE                   RDMAKEYTYPE_UnsafeGlobal

static const struct SocketOps rdmaOps = {
   .uninit = _RDMASocket_uninit,

   .connectByIP = _RDMASocket_connectByIP,
   .bindToAddr = _RDMASocket_bindToAddr,
   .listen = _RDMASocket_listen,
   .shutdown = _RDMASocket_shutdown,
   .shutdownAndRecvDisconnect = _RDMASocket_shutdownAndRecvDisconnect,

   .sendto = _RDMASocket_sendto,
   .recvT = _RDMASocket_recvT,
};

bool RDMASocket_init(RDMASocket* this, struct in_addr src, NicAddressStats* nicStats)
{
   Socket* thisBase = (Socket*)this;

   // init super class
   _PooledSocket_init( (PooledSocket*)this, NICADDRTYPE_RDMA);

   thisBase->ops = &rdmaOps;

   // normal init part

   thisBase->sockType = NICADDRTYPE_RDMA;

   this->commCfg.bufNum = RDMASOCKET_DEFAULT_BUF_NUM;
   this->commCfg.bufSize = RDMASOCKET_DEFAULT_BUF_SIZE;
   this->commCfg.fragmentSize = RDMASOCKET_DEFAULT_FRAGMENT_SIZE;
   this->commCfg.keyType = RDMASocket_toIBVSocketKeyType(RDMASOCKET_DEFAULT_KEY_TYPE);

   if(!IBVSocket_init(&this->ibvsock, src, nicStats) )
      goto err_ibv;

   return true;

err_ibv:
   _PooledSocket_uninit(&this->pooledSocket.socket);
   return false;
}

RDMASocket* RDMASocket_construct(struct in_addr src, NicAddressStats *nicStats)
{
   RDMASocket* this = kmalloc(sizeof(*this), GFP_NOFS);

   if(!this ||
      !RDMASocket_init(this, src, nicStats) )
   {
      kfree(this);
      return NULL;
   }

   return this;
}

void _RDMASocket_uninit(Socket* this)
{
   RDMASocket* thisCast = (RDMASocket*)this;

   IBVSocket_uninit(&thisCast->ibvsock);
   _PooledSocket_uninit(this);
}

bool RDMASocket_rdmaDevicesExist(void)
{
#ifdef BEEGFS_RDMA
   return true;
#else
   return false;
#endif
}

bool _RDMASocket_connectByIP(Socket* this, struct in_addr ipaddress, unsigned short port)
{
   // note: does not set the family type to the one of this socket.

   RDMASocket* thisCast = (RDMASocket*)this;

   bool connRes;

   connRes = IBVSocket_connectByIP(&thisCast->ibvsock, ipaddress, port, &thisCast->commCfg);

   if(!connRes)
   {
      // note: this message would flood the log if hosts are unreachable on the primary interface

      //char* ipStr = SocketTk_ipaddrToStr(ipaddress);
      //printk_fhgfs(KERN_WARNING, "RDMASocket failed to connect to %s.\n", ipStr);
      //kfree(ipStr);

      return false;
   }

   // connected

   // set peername if not done so already (e.g. by connect(hostname) )
   if(!this->peername[0])
   {
      SocketTk_endpointAddrToStrNoAlloc(this->peername, SOCKET_PEERNAME_LEN, ipaddress, port);
      this->peerIP = ipaddress;
   }

   return true;
}

bool _RDMASocket_bindToAddr(Socket* this, struct in_addr ipaddress, unsigned short port)
{
   RDMASocket* thisCast = (RDMASocket*)this;

   bool bindRes;

   bindRes = IBVSocket_bindToAddr(&thisCast->ibvsock, ipaddress, port);
   if(!bindRes)
   {
      //printk_fhgfs_debug(KERN_INFO, "Failed to bind RDMASocket.\n"); // debug in
      return false;
   }

   this->boundPort = port;

   return true;
}

bool _RDMASocket_listen(Socket* this)
{
   RDMASocket* thisCast = (RDMASocket*)this;

   bool listenRes;

   listenRes = IBVSocket_listen(&thisCast->ibvsock);
   if(!listenRes)
   {
      printk_fhgfs(KERN_WARNING, "Failed to set RDMASocket to listening mode.\n");
      return false;
   }

   snprintf(this->peername, SOCKET_PEERNAME_LEN, "Listen(Port: %u)", this->boundPort);

   return true;
}

bool _RDMASocket_shutdown(Socket* this)
{
   RDMASocket* thisCast = (RDMASocket*)this;

   bool shutRes = IBVSocket_shutdown(&thisCast->ibvsock);
   if(!shutRes)
   {
      printk_fhgfs_debug(KERN_INFO, "RDMASocket failed to send shutdown.\n");
      return false;
   }

   return true;
}

/**
 * Note: The RecvDisconnect-part is currently not implemented, so this is equal to the
 * normal shutdown() method.
 */
bool _RDMASocket_shutdownAndRecvDisconnect(Socket* this, int timeoutMS)
{
   return this->ops->shutdown(this);
}


/**
 * @return -ETIMEDOUT on timeout
 */
ssize_t _RDMASocket_recvT(Socket* this, struct iov_iter* iter, int flags, int timeoutMS)
{
   RDMASocket* thisCast = (RDMASocket*)this;

   ssize_t retVal;

   retVal = IBVSocket_recvT(&thisCast->ibvsock, iter, flags, timeoutMS);

   return retVal;
}

/**
 * Note: This is a connection-based socket type, so to and tolen are ignored.
 *
 * @param flags ignored
 */
ssize_t _RDMASocket_sendto(Socket* this, struct iov_iter* iter, int flags,
   fhgfs_sockaddr_in *to)
{
   RDMASocket* thisCast = (RDMASocket*)this;

   ssize_t retVal;

   retVal = IBVSocket_send(&thisCast->ibvsock, iter, flags);

   return retVal;
}

/**
 * Register for polling (=> this method does not call schedule() !).
 *
 * Note: Call this only once with finishPoll==true (=> non-blocking) or multiple times with
 *    finishPoll==true in the last call from the current thread (for cleanup).
 * Note: It's safe to call this multiple times with finishPoll==true.
 *
 * @param events the event flags you are interested in (POLL...)
 * @param finishPoll true for cleanup if you don't call poll again from this thread; (it's also ok
 *    to set this to true if you call poll only once and want to avoid blocking)
 * @return mask revents mask (like poll() => POLL... flags), but only the events you requested or
 *    error events
 */
unsigned long RDMASocket_poll(RDMASocket* this, short events, bool finishPoll)
{
   return IBVSocket_poll(&this->ibvsock, events, finishPoll);
}

