#ifndef SOCKET_H_
#define SOCKET_H_

#include <common/external/sdp_inet.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include <common/net/sock/NicAddress.h>
#include <linux/socket.h>
#include <os/iov_iter.h>


#define PF_SDP                AF_INET_SDP // the Sockets Direct Protocol (Family)
#define SOCKET_PEERNAME_LEN   24

/*
 * This is an abstract class.
 */


struct Socket;
typedef struct Socket Socket;


extern void _Socket_init(Socket* this);
extern void _Socket_uninit(Socket* this);

extern bool Socket_bind(Socket* this, unsigned short port);
extern bool Socket_bindToAddr(Socket* this, struct in_addr* ipAddr, unsigned short port);



struct SocketOps
{
   void (*uninit)(Socket* this);

   bool (*connectByIP)(Socket* this, struct in_addr* ipaddress, unsigned short port);
   bool (*bindToAddr)(Socket* this, struct in_addr* ipaddress, unsigned short port);
   bool (*listen)(Socket* this);
   bool (*shutdown)(Socket* this);
   bool (*shutdownAndRecvDisconnect)(Socket* this, int timeoutMS);

   ssize_t (*sendto)(Socket* this, struct iov_iter* iter, int flags, fhgfs_sockaddr_in *to);
   ssize_t (*recvT)(Socket* this, struct iov_iter* iter, int flags, int timeoutMS);
};

struct Socket
{
   NicAddrType_t sockType;
   char peername[SOCKET_PEERNAME_LEN];
   struct in_addr peerIP;
   int boundPort;

   const struct SocketOps* ops;

   struct {
      struct list_head _list;
      short _events;
      short revents;
   } poll;
};


static inline NicAddrType_t Socket_getSockType(Socket* this)
{
   return this->sockType;
}

static inline char* Socket_getPeername(Socket* this)
{
   return this->peername;
}

static inline struct in_addr Socket_getPeerIP(Socket* this)
{
   return this->peerIP;
}

/**
 * Calls the virtual uninit method and kfrees the object.
 */
static inline void Socket_virtualDestruct(Socket* this)
{
   this->ops->uninit(this);
   kfree(this);
}

static inline ssize_t Socket_recvT(Socket* this, struct iov_iter *iter,
      size_t length, int flags, int timeoutMS)
{
   // TODO: implementation function should accept length as well.
   struct iov_iter copy = *iter;
   iov_iter_truncate(&copy, length);

   {
      ssize_t nread = this->ops->recvT(this, &copy, flags, timeoutMS);

      if (nread >= 0)
      {
         // TODO: currently some parts of the project expect that we advance
         // the iov_iter.  But as it turns out, advancing here does not mesh
         // well with how iov_iter is supposed to be used.  A problem can be
         // observed when advancing an iov_iter of type ITER_PIPE. This will
         // result in mutation of external state (struct pipe_inode_info). IOW
         // we can't just make a copy of any iov_iter and advance that in
         // isolation.
         //
         // That means, the code should be changed such that we advance only in
         // the outermost layers of the beegfs client module.

         iov_iter_advance(iter, nread);
      }

      return nread;
   }
}

static inline ssize_t Socket_recvT_kernel(Socket* this, void *buffer,
      size_t length, int flags, int timeoutMS)
{
      struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_KVEC(buffer, length, READ);
      return this->ops->recvT(this, iter, flags, timeoutMS);
}

/**
 * Receive with timeout, extended version with numReceivedBeforeError.
 *
 * note: this uses a soft timeout that is being reset after each received data packet.
 *
 * @param outNumReceivedBeforeError number of bytes received before returning (also set in case of
 * an error, e.g. timeout); given value will only be increased and is intentionally not set to 0
 * initially.
 * @return -ETIMEDOUT on timeout.
 */
static inline ssize_t Socket_recvExactTEx(Socket* this, struct iov_iter *iter, size_t len, int flags, int timeoutMS,
   size_t* outNumReceivedBeforeError)
{
   ssize_t missingLen = len;

   do
   {
      ssize_t recvRes = this->ops->recvT(this, iter, flags, timeoutMS);

      if(unlikely(recvRes <= 0) )
         return recvRes;

      missingLen -= recvRes;
      *outNumReceivedBeforeError += recvRes;

   } while(missingLen);

   // all received if we got here
   return len;
}

static inline ssize_t Socket_recvExactTEx_kernel(Socket* this, void *buf, size_t len, int flags, int timeoutMS,
   size_t* outNumReceivedBeforeError)
{
      struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_KVEC(buf, len, READ);
      return Socket_recvExactTEx(this, iter, len, flags, timeoutMS, outNumReceivedBeforeError);
}

/**
 * Receive with timeout.
 *
 * @return -ETIMEDOUT on timeout.
 */
static inline ssize_t Socket_recvExactT(Socket* this, struct iov_iter *iter, size_t len, int flags, int timeoutMS)
{
   size_t numReceivedBeforeError;

   return Socket_recvExactTEx(this, iter, len, flags, timeoutMS, &numReceivedBeforeError);
}
static inline ssize_t Socket_recvExactT_kernel(Socket* this, void *buf, size_t len, int flags, int timeoutMS)
{
   size_t numReceivedBeforeError;

   return Socket_recvExactTEx_kernel(this, buf, len, flags, timeoutMS, &numReceivedBeforeError);
}



static inline ssize_t Socket_sendto_kernel(Socket *this, const void *buf, size_t len, int flags,
   fhgfs_sockaddr_in *to)
{
   struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_KVEC(buf, len, WRITE);
   return this->ops->sendto(this, iter, flags, to);
}

static inline ssize_t Socket_send_kernel(Socket *this, const void *buf, size_t len, int flags)
{
   return Socket_sendto_kernel(this, buf, len, flags, NULL);
}


#endif /*SOCKET_H_*/
