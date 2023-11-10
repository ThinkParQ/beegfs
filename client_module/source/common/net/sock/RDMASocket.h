#ifndef OPEN_RDMASOCKET_H_
#define OPEN_RDMASOCKET_H_

#include <common/toolkit/SocketTk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include <common/net/sock/ibv/IBVSocket.h>
#include <common/net/sock/PooledSocket.h>
#include <common/net/sock/NicAddressStats.h>

struct RDMASocket;
typedef struct RDMASocket RDMASocket;
struct NicAddressStats;
typedef struct NicAddressStats NicAddressStats;


extern __must_check bool RDMASocket_init(RDMASocket* this, struct in_addr srcIpAddr, NicAddressStats* nicStats);
extern RDMASocket* RDMASocket_construct(struct in_addr srcIpAddr, NicAddressStats* nicStats);
extern void _RDMASocket_uninit(Socket* this);

extern bool RDMASocket_rdmaDevicesExist(void);

extern bool _RDMASocket_connectByIP(Socket* this, struct in_addr ipaddress,
   unsigned short port);
extern bool _RDMASocket_bindToAddr(Socket* this, struct in_addr ipaddress,
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
#ifdef BEEGFS_NVFS
struct ib_device;
static inline struct ib_device *RDMASocket_getDevice(RDMASocket* this);
static inline unsigned RDMASocket_getKey(RDMASocket* this);
#endif

static inline void RDMASocket_setBuffers(RDMASocket* this, unsigned bufNum, unsigned bufSize,
   unsigned fragmentSize);
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

#ifdef BEEGFS_NVFS
unsigned RDMASocket_getKey(RDMASocket *this)
{
#ifndef OFED_UNSAFE_GLOBAL_RKEY
   return this->ibvsock.commContext->dmaMR->lkey;
#else
   return this->ibvsock.commContext->pd->unsafe_global_rkey;
#endif
}

struct ib_device* RDMASocket_getDevice(RDMASocket *this)
{
   return this->ibvsock.commContext->pd->device;
}
#endif /* BEEGFS_NVFS */

/**
 * Note: Only has an effect for unconnected sockets.
 */
void RDMASocket_setBuffers(RDMASocket* this, unsigned bufNum, unsigned bufSize,
   unsigned fragmentSize)
{
   this->commCfg.bufNum = bufNum;
   this->commCfg.bufSize = bufSize;
   this->commCfg.fragmentSize = fragmentSize;
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
