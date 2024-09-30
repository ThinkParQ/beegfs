#ifndef OPENTK_IBVSOCKET_H_
#define OPENTK_IBVSOCKET_H_

#include <common/Common.h>
#include <common/toolkit/Random.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include <asm/atomic.h>
#include <os/iov_iter.h>


#define IBVSOCKET_PRIVATEDATA_STR            "fhgfs0 " // must be exactly(!!) 8 bytes long
#define IBVSOCKET_PRIVATEDATA_STR_LEN        8
#define IBVSOCKET_PRIVATEDATA_PROTOCOL_VER   1

struct ib_device;
struct ib_mr;

struct IBVIncompleteRecv;
typedef struct IBVIncompleteRecv IBVIncompleteRecv;
struct IBVIncompleteSend;
typedef struct IBVIncompleteSend IBVIncompleteSend;

struct IBVCommContext;
typedef struct IBVCommContext IBVCommContext;

struct IBVCommDest;
typedef struct IBVCommDest IBVCommDest;

struct IBVTimeoutConfig;
typedef struct IBVTimeoutConfig IBVTimeoutConfig;

struct IBVSocket; // forward declaration
typedef struct IBVSocket IBVSocket;

struct IBVCommConfig;
typedef struct IBVCommConfig IBVCommConfig;

struct NicAddressStats;
typedef struct NicAddressStats NicAddressStats;

enum IBVSocketKeyType
{
   IBVSOCKETKEYTYPE_UnsafeGlobal = 0,
   IBVSOCKETKEYTYPE_UnsafeDMA,
   IBVSOCKETKEYTYPE_Register
};
typedef enum IBVSocketKeyType IBVSocketKeyType;

// construction/destruction
extern __must_check bool IBVSocket_init(IBVSocket* _this, struct in_addr srcIpAddr, NicAddressStats* nicStats);
extern void IBVSocket_uninit(IBVSocket* _this);

// static
extern bool IBVSocket_rdmaDevicesExist(void);

// methods
extern bool IBVSocket_connectByIP(IBVSocket* _this, struct in_addr ipaddress,
   unsigned short port, IBVCommConfig* commCfg);
extern bool IBVSocket_bindToAddr(IBVSocket* _this, struct in_addr ipAddr,
   unsigned short port);
extern bool IBVSocket_listen(IBVSocket* _this);
extern bool IBVSocket_shutdown(IBVSocket* _this);

extern ssize_t IBVSocket_recvT(IBVSocket* _this, struct iov_iter* iter, int flags,
   int timeoutMS);
extern ssize_t IBVSocket_send(IBVSocket* _this, struct iov_iter* iter, int flags);

extern int IBVSocket_checkConnection(IBVSocket* _this);

extern unsigned long IBVSocket_poll(IBVSocket* _this, short events, bool finishPoll);

// getters & setters
extern void IBVSocket_setTimeouts(IBVSocket* _this, int connectMS,
   int completionMS, int flowSendMS, int flowRecvMS, int pollMS);
extern void IBVSocket_setTypeOfService(IBVSocket* _this, int typeOfService);
extern void IBVSocket_setConnectionFailureStatus(IBVSocket* _this, unsigned value);
extern struct in_addr IBVSocket_getSrcIpAddr(IBVSocket* _this);

// Only access members of NicAddressStats when the owner NodeConnPool mutex is held.
// OK to access "nic" without holding mutex.
extern NicAddressStats* IBVSocket_getNicStats(IBVSocket* _this);

extern unsigned IBVSocket_getRkey(IBVSocket* _this);
extern struct ib_device* IBVSocket_getDevice(IBVSocket* _this);
extern int IBVSocket_registerMr(IBVSocket* _this, struct ib_mr* mr, int access);

struct IBVTimeoutConfig
{
   int connectMS;
   int completionMS;
   int flowSendMS;
   int flowRecvMS;
   int pollMS;
};

struct IBVCommConfig
{
   unsigned bufNum; // number of available buffers
   unsigned bufSize; // total size of each buffer
   /**
    * IBVBuffer can allocate the buffer in multiple memory regions. This
    * is to allow allocation of large buffers without requiring the
    * buffer to be entirely contiguous. A value of 0 means that the
    * buffer should not be fragmented.
    */
   unsigned fragmentSize; // size of buffer fragments
   IBVSocketKeyType keyType; // Which type of rkey for RDMA
};

#ifdef BEEGFS_RDMA
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <rdma/ib_cm.h>
#include <common/threading/Mutex.h>
#include "IBVBuffer.h"


enum IBVSocketConnState;
typedef enum IBVSocketConnState IBVSocketConnState_t;


extern bool __IBVSocket_createNewID(IBVSocket* _this);
extern bool __IBVSocket_createCommContext(IBVSocket* _this, struct rdma_cm_id* cm_id,
   IBVCommConfig* commCfg, IBVCommContext** outCommContext);
extern void __IBVSocket_cleanupCommContext(struct rdma_cm_id* cm_id, IBVCommContext* commContext);

extern bool __IBVSocket_initCommDest(IBVCommContext* commContext, IBVCommDest* outDest);
extern bool __IBVSocket_parseCommDest(const void* buf, size_t bufLen, IBVCommDest** outDest);

extern int __IBVSocket_receiveCheck(IBVSocket* _this, int timeoutMS);
extern int __IBVSocket_nonblockingSendCheck(IBVSocket* _this);

extern int __IBVSocket_postRecv(IBVSocket* _this, IBVCommContext* commContext, size_t bufIndex);
extern int __IBVSocket_postSend(IBVSocket* _this, size_t bufIndex);
extern int __IBVSocket_recvWC(IBVSocket* _this, int timeoutMS, struct ib_wc* outWC);

extern int __IBVSocket_flowControlOnRecv(IBVSocket* _this, int timeoutMS);
extern void __IBVSocket_flowControlOnSendUpdateCounters(IBVSocket* _this);
extern int __IBVSocket_flowControlOnSendWait(IBVSocket* _this, int timeoutMS);

extern int __IBVSocket_waitForRecvCompletionEvent(IBVSocket* _this, int timeoutMS,
   struct ib_wc* outWC);
extern int __IBVSocket_waitForSendCompletionEvent(IBVSocket* _this, int oldSendCount,
   int timeoutMS);
extern int __IBVSocket_waitForTotalSendCompletion(IBVSocket* _this,
   unsigned* numSendElements, unsigned* numWriteElements, unsigned* numReadElements, int timeoutMS);

extern ssize_t __IBVSocket_recvContinueIncomplete(IBVSocket* _this, struct iov_iter* iter);

extern int __IBVSocket_cmaHandler(struct rdma_cm_id* cm_id, struct rdma_cm_event* event);
extern void __IBVSocket_cqSendEventHandler(struct ib_event* event, void* data);
extern void __IBVSocket_sendCompletionHandler(struct ib_cq* cq, void* cq_context);
extern void __IBVSocket_cqRecvEventHandler(struct ib_event* event, void* data);
extern void __IBVSocket_recvCompletionHandler(struct ib_cq* cq, void* cq_context);
extern void __IBVSocket_qpEventHandler(struct ib_event* event, void* data);
extern int __IBVSocket_routeResolvedHandler(IBVSocket* _this, struct rdma_cm_id* cm_id,
   IBVCommConfig* commCfg, IBVCommContext** outCommContext);
extern int __IBVSocket_connectedHandler(IBVSocket* _this, struct rdma_cm_event *event);

extern struct ib_cq* __IBVSocket_createCompletionQueue(struct ib_device* device,
            ib_comp_handler comp_handler, void (*event_handler)(struct ib_event *, void *),
            void* cq_context, int cqe);

extern const char* __IBVSocket_wcStatusStr(int wcStatusCode);


enum IBVSocketConnState
{
   IBVSOCKETCONNSTATE_UNCONNECTED=0,
   IBVSOCKETCONNSTATE_CONNECTING=1,
   IBVSOCKETCONNSTATE_ADDRESSRESOLVED=2,
   IBVSOCKETCONNSTATE_ROUTERESOLVED=3,
   IBVSOCKETCONNSTATE_ESTABLISHED=4,
   IBVSOCKETCONNSTATE_FAILED=5,
   IBVSOCKETCONNSTATE_REJECTED_STALE=6
};


struct IBVIncompleteRecv
{
   int                  isAvailable;
   int                  completedOffset;
   int bufIndex;
   int totalSize;
};

struct IBVIncompleteSend
{
   unsigned             numAvailable;
   bool           forceWaitForAll; // true if we received only some completions and need
                                         //    to wait for the rest before we can send more data
};

struct IBVCommContext
{
   struct ib_pd*             pd; // protection domain
   struct ib_mr*             dmaMR; // system DMA MR. Not supported on all platforms.
   atomic_t                  recvCompEventCount; // incremented on incoming event notification
   wait_queue_head_t         recvCompWaitQ; // for recvCompEvents
   wait_queue_t              recvWait;
   bool                recvWaitInitialized; // true if init_wait was called for the thread
   atomic_t                  sendCompEventCount; // incremented on incoming event notification
   wait_queue_head_t         sendCompWaitQ; // for sendCompEvents
   wait_queue_t              sendWait;
   bool                sendWaitInitialized; // true if init_wait was called for the thread

   struct ib_cq*             recvCQ; // recv completion queue
   struct ib_cq*             sendCQ; // send completion queue
   struct ib_qp*             qp; // send+recv queue pair

   IBVCommConfig             commCfg;
   struct IBVBuffer*         sendBufs;
   struct IBVBuffer*         recvBufs;
   struct IBVBuffer          checkConBuffer;
   unsigned                  numReceivedBufsLeft; // flow control v2 to avoid IB rnr timeout
   unsigned                  numSendBufsLeft; // flow control v2 to avoid IB rnr timeout

   IBVIncompleteRecv         incompleteRecv;
   IBVIncompleteSend         incompleteSend;
   u32                       checkConnRkey;
};

#pragma pack(push, 1)
// Note: Make sure this struct has the same size on all architectures (because we use
//    sizeof(IBVCommDest) for private_data during handshake)
struct IBVCommDest
{
   char                 verificationStr[IBVSOCKET_PRIVATEDATA_STR_LEN];
   uint64_t             protocolVersion;
   uint64_t             vaddr;
   unsigned             rkey;
   unsigned             recvBufNum;
   unsigned             recvBufSize;
};
#pragma pack(pop)

struct IBVSocket
{
   wait_queue_head_t             eventWaitQ; // used to wait for connState change during connect


   struct rdma_cm_id*            cm_id;
   struct in_addr                srcIpAddr;

   IBVCommDest                   localDest;
   IBVCommDest*                  remoteDest;

   IBVCommContext*               commContext;

   int                           errState; // 0 = <no error>; -1 = <unspecified error>

   volatile IBVSocketConnState_t connState;

   int                           typeOfService;
   unsigned                      remapConnectionFailureStatus;
   NicAddressStats*              nicStats;  // Owned by a NodeConnPool instance. Do not access
                                            // members without locking the NodeConnPool mutex.
                                            // Possibly NULL.
   IBVTimeoutConfig              timeoutCfg;
   Mutex                         cmaMutex;  // used to manage concurrency of cm_id and commContext
                                            // with __IBVSocket_cmaHandler
};


#else


struct IBVSocket
{
   /* empty structs are not allowed, so until this kludge can go, add a dummy member */
   unsigned:0;
};


#endif

#endif /*OPENTK_IBVSOCKET_H_*/
