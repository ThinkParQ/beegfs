#ifndef NODECONNPOOL_H_
#define NODECONNPOOL_H_

#include <app/log/Logger.h>
#include <app/App.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/StandardSocket.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/SocketTk.h>
#include <common/Common.h>
#include "ConnectionList.h"
#include "ConnectionListIter.h"


// forward declaration
struct App;
struct Node;

struct NodeConnPoolStats;
typedef struct NodeConnPoolStats NodeConnPoolStats;
struct NodeConnPoolErrorState;
typedef struct NodeConnPoolErrorState NodeConnPoolErrorState;

struct NodeConnPool;
typedef struct NodeConnPool NodeConnPool;


extern void NodeConnPool_init(NodeConnPool* this, struct App* app, struct Node* parentNode,
   unsigned short streamPort, NicAddressList* nicList);
extern NodeConnPool* NodeConnPool_construct(struct App* app, struct Node* parentNode,
   unsigned short streamPort, NicAddressList* nicList);
extern void NodeConnPool_uninit(NodeConnPool* this);
extern void NodeConnPool_destruct(NodeConnPool* this);

extern Socket* NodeConnPool_acquireStreamSocket(NodeConnPool* this);
extern Socket* NodeConnPool_acquireStreamSocketEx(NodeConnPool* this, bool allowWaiting);
extern void NodeConnPool_releaseStreamSocket(NodeConnPool* this, Socket* sock);
extern void NodeConnPool_invalidateStreamSocket(NodeConnPool* this, Socket* sock);
extern unsigned NodeConnPool_disconnectAvailableStreams(NodeConnPool* this);
extern unsigned NodeConnPool_disconnectAndResetIdleStreams(NodeConnPool* this);
extern bool NodeConnPool_updateInterfaces(NodeConnPool* this, unsigned short streamPort,
   NicAddressList* nicList);

extern void __NodeConnPool_invalidateSpecificStreamSocket(NodeConnPool* this, Socket* sock);
extern unsigned __NodeConnPool_invalidateAvailableStreams(NodeConnPool* this,
   bool idleStreamsOnly, bool closeOnRelease);
extern void __NodeConnPool_resetStreamsIdleFlag(NodeConnPool* this);
extern bool __NodeConnPool_applySocketOptionsPreConnect(NodeConnPool* this, Socket* sock);
extern bool __NodeConnPool_applySocketOptionsConnected(NodeConnPool* this, Socket* sock);

extern void __NodeConnPool_statsAddNic(NodeConnPool* this, NicAddrType_t nicType);
extern void __NodeConnPool_statsRemoveNic(NodeConnPool* this, NicAddrType_t nicType);

extern void __NodeConnPool_setCompleteFail(NodeConnPool* this);
extern void __NodeConnPool_setConnSuccess(NodeConnPool* this, struct in_addr lastSuccessPeerIP,
   NicAddrType_t lastSuccessNicType);
extern bool __NodeConnPool_equalsLastSuccess(NodeConnPool* this, struct in_addr lastSuccessPeerIP,
   NicAddrType_t lastSuccessNicType);
extern bool __NodeConnPool_isLastSuccessInitialized(NodeConnPool* this);
extern bool __NodeConnPool_shouldPrintConnectedLogMsg(NodeConnPool* this,
   struct in_addr currentPeerIP, NicAddrType_t currentNicType);
extern bool __NodeConnPool_shouldPrintConnectFailedLogMsg(NodeConnPool* this,
   struct in_addr currentPeerIP, NicAddrType_t currentNicType);


extern void NodeConnPool_getStats(NodeConnPool* this, NodeConnPoolStats* outStats);
extern unsigned NodeConnPool_getFirstPeerName(NodeConnPool* this, NicAddrType_t nicType,
   ssize_t outBufLen, char* outBuf, bool* outIsNonPrimary);

// getters & setters
static inline NicAddressList* NodeConnPool_getNicList(NodeConnPool* this);
static inline void NodeConnPool_setLocalNicCaps(NodeConnPool* this,
   NicListCapabilities* localNicCaps);
static inline unsigned short NodeConnPool_getStreamPort(NodeConnPool* this);
static inline void NodeConnPool_setLogConnErrors(NodeConnPool* this, bool logConnErrors);
static inline bool __NodeConnPool_getWasLastTimeCompleteFail(NodeConnPool* this);


/**
 * Holds statistics about currently established connections.
 */
struct NodeConnPoolStats
{
   unsigned numEstablishedStd;
   unsigned numEstablishedSDP;
   unsigned numEstablishedRDMA;
};

/**
 * Holds state of failed connects to avoid log spamming with conn errors.
 */
struct NodeConnPoolErrorState
{
   struct in_addr lastSuccessPeerIP; // the last IP that we connected to successfully
   NicAddrType_t lastSuccessNicType; // the last nic type that we connected to successfully
   bool wasLastTimeCompleteFail; // true if last attempt failed on all routes
};


/**
 * This class represents a pool of stream connections to a certain node.
 */
struct NodeConnPool
{
   struct App* app;

   NicAddressList nicList;
   ConnectionList connList;

   struct Node* parentNode; // backlink to the node object which to which this conn pool belongs
   unsigned short streamPort;
   NicListCapabilities localNicCaps;

   unsigned availableConns; // available established conns
   unsigned establishedConns; // not equal to connList.size!!
   unsigned maxConns;
   unsigned fallbackExpirationSecs; // expiration time for conns to fallback interfaces
   unsigned maxConcurrentAttempts;

   NodeConnPoolStats stats;
   NodeConnPoolErrorState errState;

   bool logConnErrors; // false to disable logging during acquireStream() (e.g. for helperd)

   Mutex mutex;
   Condition changeCond;
   struct semaphore connSemaphore;
};


NicAddressList* NodeConnPool_getNicList(NodeConnPool* this)
{
   return &this->nicList;
}

/**
 * @param localNicCaps will be copied
 */
void NodeConnPool_setLocalNicCaps(NodeConnPool* this, NicListCapabilities* localNicCaps)
{
   this->localNicCaps = *localNicCaps;
}

unsigned short NodeConnPool_getStreamPort(NodeConnPool* this)
{
   unsigned short retVal;

   Mutex_lock(&this->mutex); // L O C K

   retVal = this->streamPort;

   Mutex_unlock(&this->mutex); // U N L O C K

   return retVal;
}

void NodeConnPool_setLogConnErrors(NodeConnPool* this, bool logConnErrors)
{
   this->logConnErrors = logConnErrors;
}

/**
 * @return true if connection on all available routes failed last time we tried.
 */
bool __NodeConnPool_getWasLastTimeCompleteFail(NodeConnPool* this)
{
   return this->errState.wasLastTimeCompleteFail;
}


#endif /*NODECONNPOOL_H_*/
