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
extern bool Socket_bindToAddr(Socket* this, struct in_addr* ipAddr, unsigned short port);



struct SocketOps
{
   void (*uninit)(Socket* this);

   bool (*connectByIP)(Socket* this, struct in_addr* ipaddress, unsigned short port);
   bool (*bindToAddr)(Socket* this, struct in_addr* ipaddress, unsigned short port);
   bool (*listen)(Socket* this);
   bool (*shutdown)(Socket* this);
   bool (*shutdownAndRecvDisconnect)(Socket* this, int timeoutMS);

   ssize_t (*sendto)(Socket* this, BeeGFS_IovIter* iter, int flags, fhgfs_sockaddr_in *to);
   ssize_t (*recvT)(Socket* this, BeeGFS_IovIter* iter, int flags, int timeoutMS);
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

static inline ssize_t Socket_recvT(Socket* this, BeeGFS_IovIter *iter,
      size_t length, int flags, int timeoutMS)
{
   // TODO: implementation function should accept length as well.
   BeeGFS_IovIter copy = *iter;
   beegfs_iov_iter_truncate(&copy, length);

   {
      ssize_t nread = this->ops->recvT(this, &copy, flags, timeoutMS);

      if (nread >= 0)
         beegfs_iov_iter_advance(iter, nread);

      return nread;
   }
}

static inline ssize_t Socket_recvT_kernel(Socket* this, void *buffer,
      size_t length, int flags, int timeoutMS)
{
      struct kvec kvec = {
         .iov_base = buffer,
         .iov_len = length,
      };
      BeeGFS_IovIter iter;
      BEEGFS_IOV_ITER_KVEC(&iter, READ, &kvec, 1, length);

      return this->ops->recvT(this, &iter, flags, timeoutMS);
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
static inline ssize_t Socket_recvExactTEx(Socket* this, BeeGFS_IovIter *iter, size_t len, int flags, int timeoutMS,
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
      struct kvec kvec = {
         .iov_base = buf,
         .iov_len = len,
      };
      BeeGFS_IovIter iter;
      BEEGFS_IOV_ITER_KVEC(&iter, READ, &kvec, 1, len);

      return Socket_recvExactTEx(this, &iter, len, flags, timeoutMS, outNumReceivedBeforeError);
}

/**
 * Receive with timeout.
 *
 * @return -ETIMEDOUT on timeout.
 */
static inline ssize_t Socket_recvExactT(Socket* this, BeeGFS_IovIter *iter, size_t len, int flags, int timeoutMS)
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
   struct kvec kvec = { (void *) buf, len };
   BeeGFS_IovIter iter;

   BEEGFS_IOV_ITER_KVEC(&iter, WRITE, &kvec, 1, len);

   return this->ops->sendto(this, &iter, flags, to);
}

static inline ssize_t Socket_send_kernel(Socket *this, const void *buf, size_t len, int flags)
{
   return Socket_sendto_kernel(this, buf, len, flags, NULL);
}


#endif /*SOCKET_H_*/
