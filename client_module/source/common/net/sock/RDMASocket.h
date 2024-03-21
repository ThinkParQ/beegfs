#ifndef OPEN_RDMASOCKET_H_
#define OPEN_RDMASOCKET_H_

#include <common/toolkit/SocketTk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include <common/net/sock/ibv/IBVSocket.h>
#include <common/net/sock/PooledSocket.h>
#include <common/net/sock/NicAddressStats.h>
#include <app/config/Config.h>

struct ib_device;
struct ib_mr;
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
static inline struct ib_device* RDMASocket_getDevice(RDMASocket* this);
static inline unsigned RDMASocket_getRkey(RDMASocket* this);
static inline bool RDMASocket_isRkeyGlobal(RDMASocket* this);

static inline void RDMASocket_setBuffers(RDMASocket* this, unsigned bufNum, unsigned bufSize,
   unsigned fragmentSize, RDMAKeyType keyType);
static inline void RDMASocket_setTimeouts(RDMASocket* this, int connectMS,
   int completionMS, int flowSendMS, int flowRecvMS, int pollMS);
static inline void RDMASocket_setTypeOfService(RDMASocket* this, int typeOfService);
static inline void RDMASocket_setConnectionFailureStatus(RDMASocket* this, unsigned value);
static inline bool RDMASocket_registerMr(RDMASocket* this, struct ib_mr* mr, int access);
static inline IBVSocketKeyType RDMASocket_toIBVSocketKeyType(RDMAKeyType keyType);

struct RDMASocket
{
   PooledSocket pooledSocket;

   IBVSocket ibvsock;

   IBVCommConfig commCfg;
};

unsigned RDMASocket_getRkey(RDMASocket *this)
{
   return IBVSocket_getRkey(&this->ibvsock);
}

bool RDMASocket_isRkeyGlobal(RDMASocket* this)
{
   return this->commCfg.keyType != IBVSOCKETKEYTYPE_Register;
}

struct ib_device* RDMASocket_getDevice(RDMASocket *this)
{
   return IBVSocket_getDevice(&this->ibvsock);
}

IBVSocketKeyType RDMASocket_toIBVSocketKeyType(RDMAKeyType keyType)
{
   switch (keyType)
   {
   case RDMAKEYTYPE_UnsafeDMA:
      return IBVSOCKETKEYTYPE_UnsafeDMA;
   case RDMAKEYTYPE_Register:
      return IBVSOCKETKEYTYPE_Register;
   default:
      return IBVSOCKETKEYTYPE_UnsafeGlobal;
   }
}

/**
 * Note: Only has an effect for unconnected sockets.
 */
void RDMASocket_setBuffers(RDMASocket* this, unsigned bufNum, unsigned bufSize,
   unsigned fragmentSize, RDMAKeyType keyType)
{
   this->commCfg.bufNum = bufNum;
   this->commCfg.bufSize = bufSize;
   this->commCfg.fragmentSize = fragmentSize;
   this->commCfg.keyType = RDMASocket_toIBVSocketKeyType(keyType);
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

bool RDMASocket_registerMr(RDMASocket* this, struct ib_mr* mr, int access)
{
   return !IBVSocket_registerMr(&this->ibvsock, mr, access);
}

#endif /*OPEN_RDMASOCKET_H_*/
