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

/*
 * This is an abstract class.
 */


struct Socket;
typedef struct Socket Socket;


extern void _Socket_init(Socket* this);
extern void _Socket_uninit(Socket* this);

extern bool Socket_bind(Socket* this, unsigned short port);
extern bool Socket_bindToAddr(Socket* this, struct in_addr ipAddr, unsigned short port);

// getters & setters
static inline NicAddrType_t Socket_getSockType(Socket* this);
static inline char* Socket_getPeername(Socket* this);
static inline struct in_addr Socket_getPeerIP(Socket* this);

// inliners
static inline void Socket_virtualDestruct(Socket* this);
static inline ssize_t Socket_recvExactT(Socket* this, void *buf, size_t len, int flags,
   int timeoutMS);
static inline ssize_t Socket_recvExactTEx(Socket* this, void *buf, size_t len, int flags,
   int timeoutMS, size_t* outNumReceivedBeforeError);




struct SocketOps
{
   void (*uninit)(Socket* this);

   bool (*connectByIP)(Socket* this, struct in_addr ipaddress, unsigned short port);
   bool (*bindToAddr)(Socket* this, struct in_addr ipaddress, unsigned short port);
   bool (*listen)(Socket* this);
   bool (*shutdown)(Socket* this);
   bool (*shutdownAndRecvDisconnect)(Socket* this, int timeoutMS);

   ssize_t (*sendto)(Socket* this, struct iov_iter* iter, int flags, fhgfs_sockaddr_in *to);
   ssize_t (*recvT)(Socket* this, struct iov_iter* iter, int flags, int timeoutMS);
};

struct Socket
{
   NicAddrType_t sockType;
   char* peername;
   struct in_addr peerIP;

   const struct SocketOps* ops;

   struct {
      struct list_head _list;
      short _events;
      short revents;
   } poll;
};


NicAddrType_t Socket_getSockType(Socket* this)
{
   return this->sockType;
}

char* Socket_getPeername(Socket* this)
{
   return this->peername;
}

struct in_addr Socket_getPeerIP(Socket* this)
{
   return this->peerIP;
}

/**
 * Calls the virtual uninit method and kfrees the object.
 */
void Socket_virtualDestruct(Socket* this)
{
   this->ops->uninit(this);
   kfree(this);
}

static inline ssize_t Socket_recvT(Socket* this, void* buf, size_t len, int flags, int timeoutMS)
{
   struct iovec iov = { buf, len };
   struct iov_iter iter;

   BEEGFS_IOV_ITER_INIT(&iter, READ, &iov, 1, len);

   return this->ops->recvT(this, &iter, flags, timeoutMS);
}

/**
 * Receive with timeout.
 *
 * @return -ETIMEDOUT on timeout.
 */
ssize_t Socket_recvExactT(Socket* this, void *buf, size_t len, int flags, int timeoutMS)
{
   size_t numReceivedBeforeError;

   return Socket_recvExactTEx(this, buf, len, flags, timeoutMS, &numReceivedBeforeError);
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
ssize_t Socket_recvExactTEx(Socket* this, void *buf, size_t len, int flags, int timeoutMS,
   size_t* outNumReceivedBeforeError)
{
   ssize_t missingLen = len;
   ssize_t recvRes;

   do
   {
      struct iovec iov = { buf + (len - missingLen), missingLen };
      struct iov_iter iter;

      BEEGFS_IOV_ITER_INIT(&iter, READ, &iov, 1, missingLen);

      recvRes = this->ops->recvT(this, &iter, flags, timeoutMS);

      if(unlikely(recvRes <= 0) )
         return recvRes;

      missingLen -= recvRes;
      *outNumReceivedBeforeError += recvRes;

   } while(missingLen);

   // all received if we got here

   return len;
}

static inline ssize_t Socket_sendto(Socket* this, const void* buf, size_t len, int flags,
   fhgfs_sockaddr_in* to)
{
   struct iovec iov = { (void*) buf, len };
   struct iov_iter iter;

   BEEGFS_IOV_ITER_INIT(&iter, WRITE, &iov, 1, len);

   return this->ops->sendto(this, &iter, flags, to);
}

static inline ssize_t Socket_send(Socket* this, const void* buf, size_t len, int flags)
{
   return Socket_sendto(this, buf, len, flags, NULL);
}


#endif /*SOCKET_H_*/
