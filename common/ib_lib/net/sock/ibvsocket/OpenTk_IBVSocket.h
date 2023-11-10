#ifndef OPENTK_IBVSOCKET_H_
#define OPENTK_IBVSOCKET_H_

#include <arpa/inet.h>


/*
 * This is the interface of the ibverbs socket abstraction.
 */


struct IBVSocket;
typedef struct IBVSocket IBVSocket;

struct IBVCommConfig;
typedef struct IBVCommConfig IBVCommConfig;


enum IBVSocket_AcceptRes
   {ACCEPTRES_ERR=0, ACCEPTRES_IGNORE=1, ACCEPTRES_SUCCESS=2};
typedef enum IBVSocket_AcceptRes IBVSocket_AcceptRes;


// construction/destruction
extern void IBVSocket_init(IBVSocket* _this);
extern IBVSocket* IBVSocket_construct();
extern void IBVSocket_uninit(IBVSocket* _this);
extern void IBVSocket_destruct(IBVSocket* _this);

// static
extern bool IBVSocket_rdmaDevicesExist(void);
extern void IBVSocket_fork_init_once(void);

// methods
extern bool IBVSocket_connectByName(IBVSocket* _this, const char* hostname, unsigned short port,
   IBVCommConfig* commCfg);
extern bool IBVSocket_connectByIP(IBVSocket* _this, struct in_addr ipaddress, unsigned short port,
   IBVCommConfig* commCfg);
extern bool IBVSocket_bind(IBVSocket* _this, unsigned short port);
extern bool IBVSocket_bindToAddr(IBVSocket* _this, in_addr_t ipAddr, unsigned short port);
extern bool IBVSocket_listen(IBVSocket* _this);
extern IBVSocket_AcceptRes IBVSocket_accept(IBVSocket* _this, IBVSocket** outAcceptedSock,
   struct sockaddr* peerAddr, socklen_t* peerAddrLen);
extern bool IBVSocket_shutdown(IBVSocket* _this);

#ifdef BEEGFS_NVFS
extern ssize_t IBVSocket_write(IBVSocket* _this, const char* buf, size_t bufLen, unsigned lkey,
   const uint64_t rbuf, unsigned rkey);
extern ssize_t IBVSocket_read(IBVSocket* _this, const char* buf, size_t bufLen, unsigned lkey,
   const uint64_t rbuf, unsigned rkey);
#endif /* BEEGFS_NVFS */

extern ssize_t IBVSocket_recv(IBVSocket* _this, char* buf, size_t bufLen, int flags);
extern ssize_t IBVSocket_recvT(IBVSocket* _this, char* buf, size_t bufLen, int flags,
   int timeoutMS);
extern ssize_t IBVSocket_send(IBVSocket* _this, const char* buf, size_t bufLen, int flags);

extern int IBVSocket_checkConnection(IBVSocket* _this);
extern ssize_t IBVSocket_nonblockingRecvCheck(IBVSocket* _this);
extern bool IBVSocket_checkDelayedEvents(IBVSocket* _this);


// getters & setters
extern bool IBVSocket_getSockValid(IBVSocket* _this);
extern int IBVSocket_getRecvCompletionFD(IBVSocket* _this);
extern int IBVSocket_getConnManagerFD(IBVSocket* _this);
extern void IBVSocket_setTypeOfService(IBVSocket* _this, uint8_t typeOfService);
extern void IBVSocket_setTimeouts(IBVSocket* _this, int connectMS, int flowSendMS,
   int pollMS);

// testing methods
extern void IBVSocket_setConnectionRejectionRate(IBVSocket* _this, unsigned rate);
extern bool IBVSocket_connectionRejection(IBVSocket* _this);


struct IBVCommConfig
{
   unsigned bufNum; // number of available buffers
   unsigned bufSize; // size of each buffer
   uint8_t serviceLevel;
};


#endif /*OPENTK_IBVSOCKET_H_*/
