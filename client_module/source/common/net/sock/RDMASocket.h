#ifndef OPEN_RDMASOCKET_H_
#define OPEN_RDMASOCKET_H_

#include <common/toolkit/SocketTk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include <common/net/sock/ibv/IBVSocket.h>
#include <common/net/sock/PooledSocket.h>


struct RDMASocket;
typedef struct RDMASocket RDMASocket;


extern __must_check bool RDMASocket_init(RDMASocket* this);
extern RDMASocket* RDMASocket_construct(void);
extern void _RDMASocket_uninit(Socket* this);

extern bool RDMASocket_rdmaDevicesExist(void);

extern bool _RDMASocket_connectByIP(Socket* this, struct in_addr* ipaddress,
   unsigned short port);
extern bool _RDMASocket_bindToAddr(Socket* this, struct in_addr* ipaddress,
   unsigned short port);
extern bool _RDMASocket_listen(Socket* this);
extern bool _RDMASocket_shutdown(Socket* this);
extern bool _RDMASocket_shutdownAndRecvDisconnect(Socket* this, int timeoutMS);

extern ssize_t _RDMASocket_recvT(Socket* this, struct iov_iter* iter, int flags,
   int timeoutMS);
extern ssize_t _RDMASocket_sendto(Socket* this, struct iov_iter* iter, int flags,
   fhgfs_sockaddr_in *to);

extern unsigned long RDMASocket_poll(RDMASocket* this, short events, bool finishPoll);

// inliners
static inline void RDMASocket_setBuffers(RDMASocket* this, unsigned bufNum, unsigned bufSize);
static inline void RDMASocket_setTimeouts(RDMASocket* this, int connectMS,
   int completionMS, int flowSendMS, int flowRecvMS, int pollMS);
static inline void RDMASocket_setTypeOfService(RDMASocket* this, int typeOfService);
static inline void RDMASocket_setConnectionFailureStatus(RDMASocket* this, unsigned value);

struct RDMASocket
{
   PooledSocket pooledSocket;

   IBVSocket ibvsock;

   IBVCommConfig commCfg;
};

/**
 * Note: Only has an effect for unconnected sockets.
 */
void RDMASocket_setBuffers(RDMASocket* this, unsigned bufNum, unsigned bufSize)
{
   this->commCfg.bufNum = bufNum;
   this->commCfg.bufSize = bufSize;
}

void RDMASocket_setTimeouts(RDMASocket* this, int connectMS,
   int completionMS, int flowSendMS, int flowRecvMS, int pollMS)
{
   IBVSocket_setTimeouts(&this->ibvsock, connectMS, completionMS, flowSendMS,
      flowRecvMS, pollMS);
}

/**
 * Note: Only has an effect for unconnected sockets.
 */
void RDMASocket_setTypeOfService(RDMASocket* this, int typeOfService)
{
   IBVSocket_setTypeOfService(&this->ibvsock, typeOfService);
}

/**
 * Note: Only has an effect for unconnected sockets.
 */
void RDMASocket_setConnectionFailureStatus(RDMASocket* this, unsigned value)
{
   IBVSocket_setConnectionFailureStatus(&this->ibvsock, value);
}

#endif /*OPEN_RDMASOCKET_H_*/
