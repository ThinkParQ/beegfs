#ifndef IBVSOCKET_H_
#define IBVSOCKET_H_

#include <common/toolkit/serialization/Serialization.h>
#include <common/Common.h>
#include <net/sock/ibvsocket/OpenTk_IBVSocket.h>

#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <queue>



#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#ifdef BEEGFS_NVFS
#include <common/threading/Mutex.h>
#include <unordered_map>
#endif /* BEEGFS_NVFS */

#define IBVSOCKET_RECV_WORK_ID_OFFSET        (1)
#define IBVSOCKET_SEND_WORK_ID_OFFSET        (1 + IBVSOCKET_RECV_WORK_ID_OFFSET)
#define IBVSOCKET_WRITE_WORK_ID              (1 + IBVSOCKET_SEND_WORK_ID_OFFSET)
#define IBVSOCKET_READ_WORK_ID               (1 + IBVSOCKET_WRITE_WORK_ID)

#define IBVSOCKET_EVENTS_GATHER_NUM          (64)

#define IBVSOCKET_PRIVATEDATA_STR            "fhgfs0 " // must be exactly(!!) 8 bytes long
#define IBVSOCKET_PRIVATEDATA_STR_LEN        8
#define IBVSOCKET_PRIVATEDATA_PROTOCOL_VER   1


struct IBVIncompleteRecv;
typedef struct IBVIncompleteRecv IBVIncompleteRecv;
struct IBVIncompleteSend;
typedef struct IBVIncompleteSend IBVIncompleteSend;

struct IBVCommContext;
typedef struct IBVCommContext IBVCommContext;

struct IBVCommDest;
typedef struct IBVCommDest IBVCommDest;

typedef std::queue<struct rdma_cm_event*> CmEventQueue;
#ifdef BEEGFS_NVFS
typedef std::unordered_map<char *, struct ibv_mr *> MRMap;
typedef std::unordered_map<uint64_t, int> CQMap;
#endif /* BEEGFS_NVFS */


extern void __IBVSocket_initFromCommContext(IBVSocket* _this, struct rdma_cm_id* cm_id,
   IBVCommContext* commContext);
extern IBVSocket* __IBVSocket_constructFromCommContext(struct rdma_cm_id* cm_id,
   IBVCommContext* commContext);


extern int __IBVSocket_registerBuf(IBVCommContext* commContext, void* buf, size_t bufLen,
   struct ibv_mr **outMR);
extern char* __IBVSocket_allocAndRegisterBuf(IBVCommContext* commContext, size_t bufLen,
   struct ibv_mr **outMR);

extern bool __IBVSocket_createCommContext(IBVSocket* _this, struct rdma_cm_id* cm_id,
   IBVCommConfig* commCfg, IBVCommContext** outCommContext);
extern void __IBVSocket_cleanupCommContext(struct rdma_cm_id* cm_id, IBVCommContext* commContext);

extern void __IBVSocket_initCommDest(IBVCommContext* commContext, IBVCommDest* outDest);
extern bool __IBVSocket_parseCommDest(const void* buf, size_t bufLen, IBVCommDest** outDest);

extern int __IBVSocket_postRecv(IBVSocket* _this, IBVCommContext* commContext, size_t bufIndex);
extern int __IBVSocket_postWrite(IBVSocket* _this, IBVCommDest* remoteDest,
   struct ibv_mr* localMR, char* localBuf, int bufLen);
extern int __IBVSocket_postRead(IBVSocket* _this, IBVCommDest* remoteDest,
   struct ibv_mr* localMR, char* localBuf, int bufLen);
#ifdef BEEGFS_NVFS
extern int __IBVSocket_postWrite(IBVSocket* _this, char* localBuf, int bufLen, unsigned lkey,
   uint64_t remoteBuf, unsigned rkey);
extern int __IBVSocket_postRead(IBVSocket* _this, char* localBuf, int bufLen, unsigned lkey,
   uint64_t remoteBuf, unsigned rkey);
#endif /* BEEGFS_NVFS */
extern int __IBVSocket_postSend(IBVSocket* _this, size_t bufIndex, int bufLen);
extern int __IBVSocket_recvWC(IBVSocket* _this, int timeoutMS, struct ibv_wc* outWC);

extern int __IBVSocket_flowControlOnRecv(IBVSocket* _this, int timeoutMS);
extern void __IBVSocket_flowControlOnSendUpdateCounters(IBVSocket* _this);
extern int __IBVSocket_flowControlOnSendWait(IBVSocket* _this, int timeoutMS);

extern int __IBVSocket_waitForRecvCompletionEvent(IBVSocket* _this, int timeoutMS,
   struct ibv_wc* outWC);
extern int __IBVSocket_waitForTotalSendCompletion(IBVSocket* _this,
   int numSendElements, int numWriteElements, int numReadElements);
extern int __IBVSocket_waitForUsedSendBufsReset(IBVSocket* _this);

extern ssize_t __IBVSocket_recvContinueIncomplete(IBVSocket* _this,
   char* buf, size_t bufLen);

extern void __IBVSocket_disconnect(IBVSocket* _this);
extern void __IBVSocket_close(IBVSocket* _this);

extern bool __IBVSocket_initEpollFD(IBVSocket* _this);

extern const char* __IBVSocket_wcStatusStr(int wcStatusCode);

struct IBVIncompleteRecv
{
   int                  isAvailable;
   int                  completedOffset;
   struct               ibv_wc wc;
};

struct IBVIncompleteSend
{
   unsigned             numAvailable;
};

struct IBVTimeoutConfig
{
   int                  connectMS;
   int                  flowSendMS;
   int                  pollMS;
};

struct IBVCommContext
{
   struct ibv_context*        context;
   struct ibv_pd*             pd; // protection domain
   struct ibv_mr*             recvMR; // recvBuf mem region
   struct ibv_mr*             sendMR; // sendBuf mem region
   struct ibv_mr*             controlMR; // flow/flood control mem region
   struct ibv_mr*             controlResetMR; // flow/flood control reset mem region

   struct ibv_comp_channel*   recvCompChannel; // recv completion event channel
   unsigned                   numUnackedRecvCompChannelEvents; // number of gathered events

   struct ibv_cq*             recvCQ; // recv completion queue
   struct ibv_cq*             sendCQ; // send completion queue
   struct ibv_qp*             qp; // send+recv queue pair

   IBVCommConfig              commCfg;
   char*                      recvBuf; // large alloc'ed and reg'ed buffer for recvBufs
   char**                     recvBufs; // points to chunks inside recvBuf
   char*                      sendBuf; // large alloc'ed and reg'ed buffer for sendBufs
   char**                     sendBufs; // points to chunks inside sendBuf
   volatile uint64_t          numUsedSendBufs; // sender's flow/flood control counter (volatile!!)
   volatile uint64_t          numUsedSendBufsReset; // flow/flood control reset value
   uint64_t                   numUsedRecvBufs; // receiver's flow/flood control (reset) counter
   unsigned                   numReceivedBufsLeft; // flow control v2 to avoid IB rnr timeout
   unsigned                   numSendBufsLeft; // flow control v2 to avoid IB rnr timeout

   IBVIncompleteRecv          incompleteRecv;
   IBVIncompleteSend          incompleteSend;
#ifdef BEEGFS_NVFS
   uint64_t                   wr_id;
   Mutex                      *cqMutex;
   CQMap                      *cqCompletions;
   MRMap                      *workerMRs;
#endif /* BEEGFS_NVFS */
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
   struct rdma_event_channel*    cm_channel;
   struct rdma_cm_id*            cm_id;

   IBVCommDest                   localDest;
   IBVCommDest*                  remoteDest;

   IBVCommContext*               commContext;
   int                           epollFD; // only for connected sockets, invalid (-1) for listeners

   bool                          sockValid;
   int                           errState;

   CmEventQueue*                 delayedCmEventsQ;

   uint8_t                       typeOfService;

   unsigned                      connectionRejectionRate;
   unsigned                      connectionRejectionCount;

   IBVTimeoutConfig              timeoutCfg;
   struct in_addr                bindIP;
};


#endif /*IBVSOCKET_H_*/
