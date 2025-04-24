#include <common/net/sock/StandardSocket.h>
#include <common/toolkit/Serialization.h>
#include <common/toolkit/SocketTk.h>
#include <common/Common.h>

#include <linux/in.h>
#include <linux/tcp.h>


#define SOCKET_LISTEN_BACKLOG                32
#define SOCKET_SHUTDOWN_RECV_BUF_LEN         32
#define STANDARDSOCKET_CONNECT_TIMEOUT_MS    5000

static const struct SocketOps standardOps = {
   .uninit = _StandardSocket_uninit,

   .connectByIP = _StandardSocket_connectByIP,
   .bindToAddr = _StandardSocket_bindToAddr,
   .listen = _StandardSocket_listen,
   .shutdown = _StandardSocket_shutdown,
   .shutdownAndRecvDisconnect = _StandardSocket_shutdownAndRecvDisconnect,

   .sendto = _StandardSocket_sendto,
   .recvT = _StandardSocket_recvT,
};

#ifdef KERNEL_HAS_SKWQ_HAS_SLEEPER
# define __sock_has_sleeper(wq) (skwq_has_sleeper(wq))
#else
# define __sock_has_sleeper(wq) (wq_has_sleeper(wq))
#endif

#if defined(KERNEL_HAS_SK_SLEEP) && !defined(KERNEL_HAS_SK_HAS_SLEEPER)
static inline int sk_has_sleeper(struct sock* sk)
{
   return sk->sk_sleep && waitqueue_active(sk->sk_sleep);
}
#endif

#if defined(KERNEL_WAKE_UP_SYNC_KEY_HAS_3_ARGUMENTS)
# define __wake_up_sync_key_m(wq, state, key) __wake_up_sync_key(wq, state, key)
#else
# define __wake_up_sync_key_m(wq, state, key) __wake_up_sync_key(wq, state, 1, key)
#endif


/* unlike linux sock_def_readable, this will also wake TASK_KILLABLE threads. we need this
 * for SocketTk_poll, which wants to wait for fatal signals only. */
#ifdef KERNEL_HAS_SK_DATA_READY_2
static void sock_readable(struct sock *sk, int len)
#else
static void sock_readable(struct sock *sk)
#endif
{
#ifdef KERNEL_HAS_SK_SLEEP
   read_lock(&sk->sk_callback_lock);
   if (sk_has_sleeper(sk))
   {
      __wake_up_sync_key_m(sk->sk_sleep, TASK_NORMAL,
            (void*) (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND));
   }
   read_unlock(&sk->sk_callback_lock);
#else
   struct socket_wq *wq;

   rcu_read_lock();
   wq = rcu_dereference(sk->sk_wq);
   if (__sock_has_sleeper(wq))
   {
      __wake_up_sync_key_m(&wq->wait, TASK_NORMAL,
            (void*) (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND));
   }
   rcu_read_unlock();
#endif
}

/* sock_def_write_space will also not wake uninterruptible threads. additionally, in newer kernels
 * it uses refcount_t for an optimization we will do not need: linux does not want to wake up
 * many writers if many of them cannot make progress. we have only a single writer. */
static void sock_write_space(struct sock *sk)
{
#ifdef KERNEL_HAS_SK_SLEEP
   read_lock(&sk->sk_callback_lock);

   if (sk_has_sleeper(sk))
   {
      __wake_up_sync_key_m(sk->sk_sleep, TASK_NORMAL,
            (void*) (POLLOUT | POLLWRNORM | POLLWRBAND));
   }

   read_unlock(&sk->sk_callback_lock);
#else
   struct socket_wq *wq;

   rcu_read_lock();

   wq = rcu_dereference(sk->sk_wq);
   if (__sock_has_sleeper(wq))
      __wake_up_sync_key_m(&wq->wait, TASK_NORMAL, (void*) (POLLOUT | POLLWRNORM | POLLWRBAND));

   rcu_read_unlock();
#endif
}

/* sock_def_wakeup, which is called for disconnects, has the same problem. */
static void sock_wakeup(struct sock *sk)
{
#ifdef KERNEL_HAS_SK_SLEEP
   read_lock(&sk->sk_callback_lock);
   if (sk_has_sleeper(sk))
      wake_up_all(sk->sk_sleep);
   read_unlock(&sk->sk_callback_lock);
#else
   struct socket_wq *wq;

   rcu_read_lock();
   wq = rcu_dereference(sk->sk_wq);
   if (__sock_has_sleeper(wq))
      wake_up_all(&wq->wait);
   rcu_read_unlock();
#endif
}

/* as does sock_def_error_report */
static void sock_error_report(struct sock *sk)
{
#ifdef KERNEL_HAS_SK_SLEEP
   read_lock(&sk->sk_callback_lock);
   if (sk_has_sleeper(sk))
      __wake_up_sync_key_m(sk->sk_sleep, TASK_NORMAL, (void*) (POLLERR));
   read_unlock(&sk->sk_callback_lock);
#else
   struct socket_wq *wq;

   rcu_read_lock();
   wq = rcu_dereference(sk->sk_wq);
   if (__sock_has_sleeper(wq))
      __wake_up_sync_key_m(&wq->wait, TASK_NORMAL, (void*) (POLLERR));
   rcu_read_unlock();
#endif
}


bool StandardSocket_init(StandardSocket* this, int domain, int type, int protocol)
{
   Socket* thisBase = (Socket*)this;

   NicAddrType_t nicType = NICADDRTYPE_STANDARD;

   // init super class
   _PooledSocket_init( (PooledSocket*)this, nicType);

   thisBase->ops = &standardOps;

   // normal init part

   this->sock = NULL;

   this->sockDomain = domain;

   return _StandardSocket_initSock(this, domain, type, protocol);
}

StandardSocket* StandardSocket_construct(int domain, int type, int protocol)
{
   StandardSocket* this = kmalloc(sizeof(*this), GFP_NOFS);

   if(!this ||
      !StandardSocket_init(this, domain, type, protocol) )
   {
      kfree(this);
      return NULL;
   }

   return this;
}

StandardSocket* StandardSocket_constructUDP(void)
{
   return StandardSocket_construct(PF_INET, SOCK_DGRAM, 0);
}

StandardSocket* StandardSocket_constructTCP(void)
{
   return StandardSocket_construct(PF_INET, SOCK_STREAM, 0);
}

void _StandardSocket_uninit(Socket* this)
{
   StandardSocket* thisCast = (StandardSocket*)this;

   _PooledSocket_uninit(this);

   if(thisCast->sock)
      sock_release(thisCast->sock);
}

bool _StandardSocket_initSock(StandardSocket* this, int domain, int type, int protocol)
{
   int createRes;

   // prepare/create socket
#ifndef KERNEL_HAS_SOCK_CREATE_KERN_NS
   createRes = sock_create_kern(domain, type, protocol, &this->sock);
#else
   createRes = sock_create_kern(&init_net, domain, type, protocol, &this->sock);
#endif
   if(createRes < 0)
   {
      //printk_fhgfs(KERN_WARNING, "Failed to create socket\n");
      return false;
   }

   __StandardSocket_setAllocMode(this, GFP_NOFS);
   this->sock->sk->sk_data_ready = sock_readable;
   this->sock->sk->sk_write_space = sock_write_space;
   this->sock->sk->sk_state_change = sock_wakeup;
   this->sock->sk->sk_error_report = sock_error_report;

   return true;
}

void __StandardSocket_setAllocMode(StandardSocket* this, gfp_t flags)
{
   this->sock->sk->sk_allocation = flags;
}


/**
 * Use this to change socket options.
 * Note: Behaves (almost) like user-space setsockopt.
 *
 * @return 0 on success, error code otherwise (=> different from userspace version)
 */
int _StandardSocket_setsockopt(StandardSocket* this, int level,
   int optname, char* optval, int optlen)
{
   struct socket *sock = this->sock;

   #if defined(KERNEL_HAS_SOCK_SETSOCKOPT_SOCKPTR_T_PARAM)

      sockptr_t ptr = KERNEL_SOCKPTR(optval);

      if (level == SOL_SOCKET)
         return sock_setsockopt(sock, level, optname, ptr, optlen);
      else
         return sock->ops->setsockopt(sock, level, optname, ptr, optlen);

   #elif defined(KERNEL_HAS_GET_FS)

      char __user *ptr = (char __user __force *) optval;
      int r;

      WITH_PROCESS_CONTEXT
         if (level == SOL_SOCKET)
            r = sock_setsockopt(sock, level, optname, ptr, optlen);
         else
            r = sock->ops->setsockopt(sock, level, optname, ptr, optlen);
      return r;

   #else
      #error need set_fs()/get_fs() if sockptr_t is not available.
   #endif

      // unreachable
      BUG();
}

bool StandardSocket_setSoKeepAlive(StandardSocket* this, bool enable)
{
   int val = (enable ? 1 : 0);

   int r = _StandardSocket_setsockopt(this, SOL_SOCKET, SO_KEEPALIVE, (char *) &val, sizeof val);

   return r == 0;
}

bool StandardSocket_setSoBroadcast(StandardSocket* this, bool enable)
{
   int val = (enable ? 1 : 0);

   int r = _StandardSocket_setsockopt(this, SOL_SOCKET, SO_BROADCAST, (char *) &val, sizeof val);

   return r == 0;
}

int StandardSocket_getSoRcvBuf(StandardSocket* this)
{
   //TODO: should this be READ_ONCE()? There are different uses in the Linux kernel
   return this->sock->sk->sk_rcvbuf;
}

/**
 * Note: Increase only (buffer will not be set to a smaller value).
 *
 * @return false on error, true otherwise (decrease skipping is not an error)
 */
bool StandardSocket_setSoRcvBuf(StandardSocket* this, int size)
{
   int origBufLen = StandardSocket_getSoRcvBuf(this);

   if (origBufLen >= size)
   {
      // we don't decrease buf sizes (but this is not an error)
      return true;
   }
   else
   {
      /* note: according to socket(7) man page, the value given to setsockopt()
       * is doubled and the doubled value is returned by getsockopt()
       *
       * update 2022-05-13: the kernel doubles the value passed to
       * setsockopt(SO_RCVBUF) to allow for bookkeeping overhead. Halving the
       * value is probably "not correct" but it's been this way since 2010 and
       * changing it will potentially do more harm than good at this point.
       */

      int val = size/2;

      int r = _StandardSocket_setsockopt(this, SOL_SOCKET, SO_RCVBUF, (char *)
            &val, sizeof val);

      if(r != 0)
         printk_fhgfs_debug(KERN_INFO, "%s: setSoRcvBuf error: %d;\n", __func__, r);

      return r == 0;
   }
}

bool StandardSocket_setTcpNoDelay(StandardSocket* this, bool enable)
{
   int val = (enable ? 1 : 0);

   int r = _StandardSocket_setsockopt(this, SOL_TCP, TCP_NODELAY, (char*) &val, sizeof val);

   return r == 0;
}

bool StandardSocket_setTcpCork(StandardSocket* this, bool enable)
{
   int val = (enable ? 1 : 0);

   int r = _StandardSocket_setsockopt(this, SOL_TCP, TCP_CORK, (char*) &val, sizeof val);

   return r == 0;
}

bool _StandardSocket_connectByIP(Socket* this, struct in_addr ipaddress, unsigned short port)
{
   // note: this might look a bit strange (it's kept similar to the c++ version)

   // note: error messages here would flood the log if hosts are unreachable on primary interface


   const int timeoutMS = STANDARDSOCKET_CONNECT_TIMEOUT_MS;

   StandardSocket* thisCast = (StandardSocket*)this;

   int connRes;

   struct sockaddr_in serveraddr =
   {
      .sin_family = AF_INET,
      .sin_addr = ipaddress,
      .sin_port = htons(port),
   };

   connRes = kernel_connect(thisCast->sock,
      (struct sockaddr*) &serveraddr,
      sizeof(serveraddr),
      O_NONBLOCK);

   if(connRes)
   {
      if(connRes == -EINPROGRESS)
      { // wait for "ready to send data"
         PollState state;
         int pollRes;

         PollState_init(&state);
         PollState_addSocket(&state, this, POLLOUT);

         pollRes = SocketTk_poll(&state, timeoutMS);

         if(pollRes > 0)
         { // we got something (could also be an error)

            /* note: it's important to test ERR/HUP/NVAL here instead of POLLOUT only, because
               POLLOUT and POLLERR can be returned together. */

            if(this->poll.revents & (POLLERR | POLLHUP | POLLNVAL) )
               return false;

            // connection successfully established

            if(!this->peername[0])
            {
               SocketTk_endpointAddrToStrNoAlloc(this->peername, SOCKET_PEERNAME_LEN, ipaddress, port);
               this->peerIP = ipaddress;
            }

            return true;
         }
         else
         if(!pollRes)
            return false; // timeout
         else
            return false; // connection error

      } // end of "EINPROGRESS"
   }
   else
   { // connected immediately

      // set peername if not done so already (e.g. by connect(hostname) )
      if(!this->peername[0])
      {
         SocketTk_endpointAddrToStrNoAlloc(this->peername, SOCKET_PEERNAME_LEN, ipaddress, port);
         this->peerIP = ipaddress;
      }

      return true;
   }

   return false;
}


bool _StandardSocket_bindToAddr(Socket* this, struct in_addr ipaddress, unsigned short port)
{
   StandardSocket* thisCast = (StandardSocket*)this;

   struct sockaddr_in bindAddr;
   int bindRes;

   bindAddr.sin_family = thisCast->sockDomain;
   bindAddr.sin_addr = ipaddress;
   bindAddr.sin_port = htons(port);

   bindRes = kernel_bind(thisCast->sock, (struct sockaddr*)&bindAddr, sizeof(bindAddr) );

   if(bindRes)
   {
      printk_fhgfs(KERN_WARNING, "Failed to bind socket. ErrCode: %d\n", bindRes);
      return false;
   }

   this->boundPort = port;

   return true;
}

bool _StandardSocket_listen(Socket* this)
{
   StandardSocket* thisCast = (StandardSocket*)this;
   int r;

   r = kernel_listen(thisCast->sock, SOCKET_LISTEN_BACKLOG);
   if(r)
   {
      printk_fhgfs(KERN_WARNING, "Failed to set socket to listening mode. ErrCode: %d\n",
         r);
      return false;
   }

   snprintf(this->peername, SOCKET_PEERNAME_LEN, "Listen(Port: %d)", this->boundPort);

   return true;
}

bool _StandardSocket_shutdown(Socket* this)
{
   StandardSocket* thisCast = (StandardSocket*)this;

   int sendshutRes;

   sendshutRes = kernel_sock_shutdown(thisCast->sock, SEND_SHUTDOWN);

   if( (sendshutRes < 0) && (sendshutRes != -ENOTCONN) )
   {
      printk_fhgfs(KERN_WARNING, "Failed to send shutdown. ErrCode: %d\n", sendshutRes);
      return false;
   }

   return true;
}

bool _StandardSocket_shutdownAndRecvDisconnect(Socket* this, int timeoutMS)
{
   bool shutRes;
   char buf[SOCKET_SHUTDOWN_RECV_BUF_LEN];
   int recvRes;

   shutRes = this->ops->shutdown(this);
   if(!shutRes)
      return false;

   // receive until shutdown arrives
   do
   {
      recvRes = Socket_recvT_kernel(this, buf, SOCKET_SHUTDOWN_RECV_BUF_LEN, 0, timeoutMS);
   } while(recvRes > 0);

   if(recvRes &&
      (recvRes != -ECONNRESET) )
   { // error occurred (but we're disconnecting, so we don't really care about errors)
      return false;
   }

   return true;
}



/* Compatibility wrappers for sock_sendmsg / sock_recvmsg. At some point in the
 * 4.x series, the size argument disappeared. */
static int beegfs_recvmsg(struct socket *sock, struct msghdr *msg, size_t len, int flags)
{
#ifdef KERNEL_HAS_RECVMSG_SIZE
   return sock_recvmsg(sock, msg, len, flags);
#else
   return sock_recvmsg(sock, msg, flags);
#endif
}
static int beegfs_sendmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
#ifdef KERNEL_HAS_RECVMSG_SIZE
   return sock_sendmsg(sock, msg, len);
#else
   return sock_sendmsg(sock, msg);
#endif
}




/**
 * @return -ETIMEDOUT on timeout
 */
ssize_t _StandardSocket_recvT(Socket* this, struct iov_iter* iter, int flags, int timeoutMS)
{
   StandardSocket* thisCast = (StandardSocket*)this;

   return StandardSocket_recvfromT(thisCast, iter, flags, NULL, timeoutMS);
}


ssize_t _StandardSocket_sendto(Socket* this, struct iov_iter* iter, int flags,
   fhgfs_sockaddr_in *to)
{
   StandardSocket* thisCast = (StandardSocket*)this;
   struct socket *sock = thisCast->sock;

   int sendRes;
   size_t len;
   struct sockaddr_in toSockAddr;

   struct msghdr msg =
   {
      .msg_control      = NULL,
      .msg_controllen   = 0,
      .msg_flags        = flags | MSG_NOSIGNAL,
      .msg_name         = (struct sockaddr*)(to ? &toSockAddr : NULL),
      .msg_namelen      = sizeof(toSockAddr),
      .msg_iter         = *iter,
   };

   len = iov_iter_count(iter);

   if (to)
   {
      toSockAddr.sin_family = thisCast->sockDomain;
      toSockAddr.sin_addr = to->addr;
      toSockAddr.sin_port = to->port;
   }

   sendRes = beegfs_sendmsg(sock, &msg, len);

   if(sendRes >= 0)
      iov_iter_advance(iter, sendRes);

   return sendRes;
}

ssize_t StandardSocket_recvfrom(StandardSocket* this, struct iov_iter* iter, int flags,
   fhgfs_sockaddr_in *from)
{
   int recvRes;
   size_t len;
   struct sockaddr_in fromSockAddr;
   struct socket *sock = this->sock;

   struct msghdr msg =
   {
      .msg_control      = NULL,
      .msg_controllen   = 0,
      .msg_flags        = flags,
      .msg_name         = (struct sockaddr*)&fromSockAddr,
      .msg_namelen      = sizeof(fromSockAddr),
      .msg_iter         = *iter,
   };

   len = iov_iter_count(iter);

   recvRes = beegfs_recvmsg(sock, &msg, len, flags);

   if(recvRes > 0)
      iov_iter_advance(iter, recvRes);

   if(from)
   {
      from->addr = fromSockAddr.sin_addr;
      from->port = fromSockAddr.sin_port;
   }

   return recvRes;
}

/**
 * @return -ETIMEDOUT on timeout
 */
ssize_t StandardSocket_recvfromT(StandardSocket* this, struct iov_iter* iter, int flags,
   fhgfs_sockaddr_in *from, int timeoutMS)
{
   Socket* thisBase = (Socket*)this;

   int pollRes;
   PollState state;

   if(timeoutMS < 0)
      return StandardSocket_recvfrom(this, iter, flags, from);

   PollState_init(&state);
   PollState_addSocket(&state, thisBase, POLLIN);

   pollRes = SocketTk_poll(&state, timeoutMS);

   if( (pollRes > 0) && (thisBase->poll.revents & POLLIN) )
      return StandardSocket_recvfrom(this, iter, flags, from);

   if(!pollRes)
      return -ETIMEDOUT;

   if(thisBase->poll.revents & POLLERR)
      printk_fhgfs_debug(KERN_DEBUG, "StandardSocket_recvfromT: poll(): %s: Error condition\n",
         thisBase->peername);
   else
   if(thisBase->poll.revents & POLLHUP)
      printk_fhgfs_debug(KERN_DEBUG, "StandardSocket_recvfromT: poll(): %s: Hung up\n",
         thisBase->peername);
   else
   if(thisBase->poll.revents & POLLNVAL)
      printk_fhgfs(KERN_DEBUG, "StandardSocket_recvfromT: poll(): %s: Invalid request\n",
         thisBase->peername);
   else
      printk_fhgfs(KERN_DEBUG, "StandardSocket_recvfromT: poll(): %s: ErrCode: %d\n",
         thisBase->peername, pollRes);

   return -ECOMM;
}
