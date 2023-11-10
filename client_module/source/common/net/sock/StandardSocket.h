#ifndef OPEN_STANDARDSOCKET_H_
#define OPEN_STANDARDSOCKET_H_

#include <common/external/sdp_inet.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include <common/net/sock/PooledSocket.h>


struct StandardSocket;
typedef struct StandardSocket StandardSocket;


extern __must_check bool StandardSocket_init(StandardSocket* this, int domain, int type,
   int protocol);
extern StandardSocket* StandardSocket_construct(int domain, int type, int protocol);
extern StandardSocket* StandardSocket_constructUDP(void);
extern StandardSocket* StandardSocket_constructTCP(void);
extern StandardSocket* StandardSocket_constructSDP(void);
extern void _StandardSocket_uninit(Socket* this);

int StandardSocket_getSoRcvBuf(StandardSocket* this);
extern bool StandardSocket_setSoKeepAlive(StandardSocket* this, bool enable);
extern bool StandardSocket_setSoBroadcast(StandardSocket* this, bool enable);
extern bool StandardSocket_setSoRcvBuf(StandardSocket* this, int size);
extern bool StandardSocket_setTcpNoDelay(StandardSocket* this, bool enable);
extern bool StandardSocket_setTcpCork(StandardSocket* this, bool enable);

extern bool _StandardSocket_connectByIP(Socket* this, struct in_addr ipaddress,
   unsigned short port);
extern bool _StandardSocket_bindToAddr(Socket* this, struct in_addr ipaddress,
   unsigned short port);
extern bool _StandardSocket_listen(Socket* this);
extern bool _StandardSocket_shutdown(Socket* this);
extern bool _StandardSocket_shutdownAndRecvDisconnect(Socket* this, int timeoutMS);

extern ssize_t _StandardSocket_recvT(Socket* this, struct iov_iter* iter, int flags,
   int timeoutMS);
extern ssize_t _StandardSocket_sendto(Socket* this, struct iov_iter* iter, int flags,
   fhgfs_sockaddr_in *to);

extern ssize_t StandardSocket_recvfrom(StandardSocket* this, struct iov_iter* iter,
   int flags, fhgfs_sockaddr_in *from);
extern ssize_t StandardSocket_recvfromT(StandardSocket* this, struct iov_iter* iter,
   int flags, fhgfs_sockaddr_in *from, int timeoutMS);

extern bool _StandardSocket_initSock(StandardSocket* this, int domain, int type,
   int protocol);
extern void __StandardSocket_setAllocMode(StandardSocket* this, gfp_t flags);
extern int _StandardSocket_setsockopt(StandardSocket* this, int level, int optname, char* optval,
   int optlen);

// getters & setters
static inline struct socket* StandardSocket_getRawSock(StandardSocket* this);

struct StandardSocket
{
   PooledSocket pooledSocket;
   struct socket* sock;
   unsigned short sockDomain;
};

struct socket* StandardSocket_getRawSock(StandardSocket* this)
{
   return this->sock;
}


#endif /*OPEN_STANDARDSOCKET_H_*/
