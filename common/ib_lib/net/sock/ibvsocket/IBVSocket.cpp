#include "IBVSocket.h"

#include <sys/epoll.h>

#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>

#ifdef BEEGFS_NVFS
// only for WORKER_BUFOUT_SIZE
#include <common/components/worker/Worker.h>
#endif /* BEEGFS_NVFS */

#define IBVSOCKET_CONN_TIMEOUT_MS                  3000
// IBVSOCKET_CONN_TIMEOUT_POLL_MS must be < 1000
#define IBVSOCKET_CONN_TIMEOUT_POLL_MS              500
#define IBVSOCKET_FLOWCONTROL_ONSEND_TIMEOUT_MS  180000
#define IBVSOCKET_POLL_TIMEOUT_MS                  7500
#define IBVSOCKET_LISTEN_BACKLOG                    128
#define IBVSOCKET_FLOWCONTROL_MSG_LEN                 1
#define IBVSOCKET_DEFAULT_TOS                         0
/**
 * IBVSOCKET_RECV_TIMEOUT_MS is used by IBVSocket_recv, which does not take a
 * timeout value. It is very long because IBVSocket_recv continues to call
 * IBVSocket_recvT until it does not timeout.
 */
#define IBVSOCKET_RECV_TIMEOUT_MS           (1024*1024)

#define IBVSOCKET_MIN_BUF_NUM                         1
#define IBVSOCKET_MIN_BUF_SIZE                        4096   // 4kiB
#define IBVSOCKET_MAX_BUF_SIZE_NUM                    134217728 // num * size <= 128MiB
#ifdef BEEGFS_NVFS
#define IBVSOCKET_WC_ENTRIES 1
#endif /* BEEGFS_NVFS */

void IBVSocket_init(IBVSocket* _this)
{
   memset(_this, 0, sizeof(*_this) );

   _this->sockValid = false;
   _this->epollFD = -1;
   _this->typeOfService = IBVSOCKET_DEFAULT_TOS;
   _this->timeoutCfg.connectMS = IBVSOCKET_CONN_TIMEOUT_MS;
   _this->timeoutCfg.flowSendMS = IBVSOCKET_FLOWCONTROL_ONSEND_TIMEOUT_MS;

   _this->cm_channel = rdma_create_event_channel();
   if(!_this->cm_channel)
   {
      LOG(SOCKLIB, WARNING, "rdma_create_event_channel failed.");
      return;
   }

   if(rdma_create_id(_this->cm_channel, &_this->cm_id, NULL, RDMA_PS_TCP) )
   {
      LOG(SOCKLIB, WARNING, "rdma_create_id failed.");
      return;
   }

   _this->sockValid = true;

   return;
}

/**
 * Note: Intended for incoming accepted connections.
 *
 * @param commContext belongs to this object (so do not use or free it after calling this!)
 */
void __IBVSocket_initFromCommContext(IBVSocket* _this, struct rdma_cm_id* cm_id,
   IBVCommContext* commContext)
{
   memset(_this, 0, sizeof(*_this) );

   _this->sockValid = false;
   _this->epollFD = -1;

   _this->typeOfService = IBVSOCKET_DEFAULT_TOS;

   _this->cm_id = cm_id;
   _this->commContext = commContext;

   #ifdef SYSTEM_HAS_RDMA_MIGRATE_ID__disabled

   // note: see _accept() for the reasons why this is currently disabled

   _this->cm_channel = rdma_create_event_channel();
   if(!_this->cm_channel)
   {
      LOG(SOCKLIB, WARNING, "rdma_create_event_channel failed.");
      return;
   }

   #endif // SYSTEM_HAS_RDMA_MIGRATE_ID

   _this->sockValid = true;
   LOG(SOCKLIB, DEBUG, __func__,
      ("_this", StringTk::uint64ToHexStr((uint64_t) _this)),
      ("device", cm_id->verbs->device->name));

   return;
}

IBVSocket* IBVSocket_construct()
{
   IBVSocket* _this = (IBVSocket*)malloc(sizeof(*_this) );

   IBVSocket_init(_this);

   return _this;
}

IBVSocket* __IBVSocket_constructFromCommContext(struct rdma_cm_id* cm_id,
   IBVCommContext* commContext)
{
   IBVSocket* _this = (IBVSocket*)malloc(sizeof(*_this) );

   __IBVSocket_initFromCommContext(_this, cm_id, commContext);

   return _this;
}

void IBVSocket_uninit(IBVSocket* _this)
{
   if(_this->epollFD != -1)
      close(_this->epollFD);

   __IBVSocket_close(_this);
}

void IBVSocket_destruct(IBVSocket* _this)
{
   IBVSocket_uninit(_this);

   free(_this);
}

bool IBVSocket_rdmaDevicesExist()
{
   bool devicesExist;

   int numDevices = 1;
   struct ibv_context** devicesRes;

   devicesRes = rdma_get_devices(&numDevices);

   devicesExist = (devicesRes != NULL) && (numDevices > 0);

   if(devicesRes)
      rdma_free_devices(devicesRes);

   return devicesExist;
}

/**
 * Prepare ibverbs for forking a child process. This is only required if the parent process
 * has mapped memory for RDMA.
 * Call this only once in your program.
 *
 * Note: There is no corresponding uninit-method that needs to be called.
 */
void IBVSocket_fork_init_once()
{
   ibv_fork_init();
}

bool IBVSocket_connectByName(IBVSocket* _this, const char* hostname, unsigned short port,
   IBVCommConfig* commCfg)
{
   struct addrinfo *res;
   struct addrinfo hints;

   int getInfoRes;
   struct in_addr ipaddress;

   memset(&hints, 0, sizeof(hints) );
   hints.ai_family = PF_INET;
   hints.ai_socktype = SOCK_STREAM;

   getInfoRes = getaddrinfo(hostname, NULL, &hints, &res);

   if(getInfoRes < 0)
   {
      LOG(SOCKLIB, WARNING, "Name resolution error.", hostname, port,
            ("error", gai_strerror(getInfoRes)));

      return false;
   }

   ipaddress.s_addr = ( (struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;


   // clean-up
   freeaddrinfo(res);


   return IBVSocket_connectByIP(_this, ipaddress, port, commCfg);
}

bool IBVSocket_connectByIP(IBVSocket* _this, struct in_addr ipaddress, unsigned short port,
   IBVCommConfig* commCfg)
{
   struct rdma_cm_event* event;
   struct sockaddr_in sin;
   bool createContextRes;
   struct rdma_conn_param conn_param;
   bool parseCommDestRes;
   bool epollInitRes;
   int rc;
   int connTimeoutRemaining = IBVSOCKET_CONN_TIMEOUT_MS;
   int oldChannelFlags;
   int setOldFlagsRes;

   LOG(SOCKLIB, DEBUG, "Connect RDMASocket", ("socket", _this), ("addr", Socket::endpointAddrToStr(ipaddress, port)),
      ("bindIP", Socket::ipaddrToStr(_this->bindIP)));

   // resolve IP address...

   sin.sin_addr.s_addr = ipaddress.s_addr;
   sin.sin_family = AF_INET;
   sin.sin_port = htons(port);

   if(rdma_resolve_addr(_this->cm_id, NULL, (struct sockaddr*)&sin, _this->timeoutCfg.connectMS) )
   {
      LOG(SOCKLIB, WARNING, "rdma_resolve_addr failed.");
      goto err_invalidateSock;
   }

   if(rdma_get_cm_event(_this->cm_channel, &event))
      goto err_invalidateSock;

   if(event->event != RDMA_CM_EVENT_ADDR_RESOLVED)
   {
      LOG(SOCKLIB, DEBUG, "Unexpected CM event.", ("event", rdma_event_str(event->event)));
      goto err_ack_and_invalidateSock;
   }

   rdma_ack_cm_event(event);

   // set type of service for connection
   if (_this->typeOfService)
   {
      if (rdma_set_option(_this->cm_id, RDMA_OPTION_ID, RDMA_OPTION_ID_TOS, &(_this->typeOfService),
            sizeof(_this->typeOfService)))
      {
         LOG(SOCKLIB, WARNING, "Failed to set Type Of Service.",
               ("tos", _this->typeOfService));
         goto err_invalidateSock;
      }
   }

   // resolve route...

   if(rdma_resolve_route(_this->cm_id, _this->timeoutCfg.connectMS) )
   {
      LOG(SOCKLIB, WARNING, "rdma_resolve_route failed.");
      goto err_invalidateSock;
   }

   if(rdma_get_cm_event(_this->cm_channel, &event))
      goto err_invalidateSock;

   if(event->event != RDMA_CM_EVENT_ROUTE_RESOLVED)
   {
      LOG(SOCKLIB, WARNING, "Unexpected CM event.",
            ("event", rdma_event_str(event->event)));
      goto err_ack_and_invalidateSock;
   }

   rdma_ack_cm_event(event);

   // create comm context...

   createContextRes = __IBVSocket_createCommContext(_this, _this->cm_id, commCfg,
      &_this->commContext);
   if(!createContextRes)
   {
      LOG(SOCKLIB, WARNING, "creation of CommContext failed.");
      goto err_invalidateSock;
   }

   // establish connection...

   __IBVSocket_initCommDest(_this->commContext, &_this->localDest);

   memset(&conn_param, 0, sizeof(conn_param) );
#ifdef BEEGFS_NVFS
   conn_param.responder_resources = RDMA_MAX_RESP_RES;
   conn_param.initiator_depth = RDMA_MAX_INIT_DEPTH;
#else
   conn_param.responder_resources = 1;
   conn_param.initiator_depth = 1;
#endif /* BEEGFS_NVFS */
   conn_param.flow_control = 0;
   conn_param.retry_count = 7; // (3 bits)
   conn_param.rnr_retry_count = 7; // rnr = receiver not ready (3 bits, 7 means infinity)
   conn_param.private_data = &_this->localDest;
   conn_param.private_data_len = sizeof(_this->localDest);

   if(rdma_connect(_this->cm_id, &conn_param))
   {
      LOG(SOCKLIB, DEBUG, "rdma_connect failed.");
      goto err_invalidateSock;
   }

   oldChannelFlags = fcntl(IBVSocket_getConnManagerFD(_this), F_GETFL);

   rc = fcntl(IBVSocket_getConnManagerFD(_this), F_SETFL, oldChannelFlags | O_NONBLOCK);
   if(rc < 0)
   {
      LOG(SOCKLIB, WARNING, "Set conn manager channel non-blocking failed.", sysErr);
      goto err_invalidateSock;
   }

   // rdma_connect() can take a very long time (>5m) to timeout if the peer's HCA is down.
   // Change the channel to non-blocking and use a custom timeout mechanism.
   rc = -1;
   while (connTimeoutRemaining > 0)
   {
      // (non-blocking) check for new events
      rc = rdma_get_cm_event(_this->cm_channel, &event);

      if (rc)
      {
         if (errno != ETIMEDOUT && errno != EAGAIN)
         {
            LOG(SOCKLIB, WARNING, "rdma_get_cm_event failed", ("errno", errno));
            break;
         }
      }
      else
      {
         // we got an event
         break;
      }

      connTimeoutRemaining -= IBVSOCKET_CONN_TIMEOUT_POLL_MS;
      if (connTimeoutRemaining > 0)
      {
         struct timespec ts = {
            .tv_sec = 0,
            .tv_nsec = (IBVSOCKET_CONN_TIMEOUT_POLL_MS * 1000 * 1000)
         };

         if (::nanosleep(&ts, NULL) != 0)
         {
            LOG(SOCKLIB, DEBUG, "rdma_connect: sleep interrupted");
            break;
         }
      }
      else
         LOG(SOCKLIB, DEBUG, "rdma_connect: timed out");
   }

   // change channel mode back to blocking
   setOldFlagsRes = fcntl(IBVSocket_getConnManagerFD(_this), F_SETFL, oldChannelFlags);
   if(setOldFlagsRes < 0)
   {
      LOG(SOCKLIB, WARNING, "Set conn manager channel blocking failed.", sysErr);
      if (rc == 0)
         goto err_ack_and_invalidateSock;
      else
         goto err_invalidateSock;
   }

   if (rc != 0)
      goto err_invalidateSock;

   if(event->event != RDMA_CM_EVENT_ESTABLISHED)
   {
      if(event->event == RDMA_CM_EVENT_REJECTED)
         LOG(SOCKLIB, DEBUG, "Connection rejected.");
      else
         LOG(SOCKLIB, WARNING, "Unexpected conn manager event.",
               ("event", rdma_event_str(event->event)));
      goto err_ack_and_invalidateSock;
   }

   parseCommDestRes = __IBVSocket_parseCommDest(
      event->param.conn.private_data, event->param.conn.private_data_len, &_this->remoteDest);
   if(!parseCommDestRes)
   {
      LOG(SOCKLIB, WARNING, "Bad private data received.",
            ("len", event->param.conn.private_data_len));
      goto err_ack_and_invalidateSock;
   }

   rdma_ack_cm_event(event);

   epollInitRes = __IBVSocket_initEpollFD(_this);
   if(!epollInitRes)
      goto err_invalidateSock;

   return true;


err_ack_and_invalidateSock:
   rdma_ack_cm_event(event);
err_invalidateSock:
   _this->errState = -1;

   return false;
}

/**
 * @return true on success
 */
bool IBVSocket_bind(IBVSocket* _this, unsigned short port)
{
   in_addr_t ipAddr = INADDR_ANY;

   return IBVSocket_bindToAddr(_this, ipAddr, port);
}

bool IBVSocket_bindToAddr(IBVSocket* _this, in_addr_t ipAddr, unsigned short port)
{
   struct sockaddr_in bindAddr;

   bindAddr.sin_family = AF_INET;
   bindAddr.sin_addr.s_addr = ipAddr;
   bindAddr.sin_port = htons(port);

   LOG(SOCKLIB, DEBUG, "Bind RDMASocket", ("socket", _this), ("addr", Socket::endpointAddrToStr(ipAddr, port)));

   if(rdma_bind_addr(_this->cm_id, (struct sockaddr*)&bindAddr) )
   {
      //SyslogLogger::log(LOG_WARNING, "%s:%d rdma_bind_addr failed (port: %d)\n",
         //__func__, __LINE__,  (int)port); // debug in
      goto err_invalidateSock;
   }

   _this->bindIP.s_addr = ipAddr;

   return true;


err_invalidateSock:
   _this->errState = -1;

   return false;
}

/**
 * Note: This also inits the delayedCmEventsQueue.
 *
 * @return true on success
 */
bool IBVSocket_listen(IBVSocket* _this)
{
   if(rdma_listen(_this->cm_id, IBVSOCKET_LISTEN_BACKLOG) )
   {
      LOG(SOCKLIB, WARNING, "rdma_listen failed.");
      goto err_invalidateSock;
   }

   // init delayed events queue
   _this->delayedCmEventsQ = new CmEventQueue();


   return true;


err_invalidateSock:
   _this->errState = -1;

   return false;
}

/**
 * Note: Call IBVSocket_checkDelayedEvents() after this to find out whether more events
 *    are waiting.
 * Note: Because of the special way ibverbs accept connections, it is possible that we receive
 *    some other events here as well (e.g. a child socket disconnect). In these cases,
 *    ACCEPTRES_IGNORE will be returned.
 *
 * @param outAcceptedSock only valid when ACCEPTRES_SUCCESS is returned
 * @param peerAddr (out) peer address
 * @param peerAddrLen (out) length of peer address
 * @return ACCEPTRES_IGNORE in case an irrelevant event occurred
 */
IBVSocket_AcceptRes IBVSocket_accept(IBVSocket* _this, IBVSocket** outAcceptedSock,
   struct sockaddr* peerAddr, socklen_t* peerAddrLen)
{
   struct rdma_cm_event* event = NULL;
   IBVCommContext* childCommContext = NULL;
   IBVSocket* acceptedSock = NULL; // auto-destructed on error/ignore (internal, not for caller)
   IBVCommDest* childRemoteDest = NULL; // auto-freed on error/ignore

   *outAcceptedSock = NULL;


   // get next waiting event from delay-queue or from event channel

   if (!_this->delayedCmEventsQ->empty())
   {
      event = _this->delayedCmEventsQ->front();
      _this->delayedCmEventsQ->pop();
   }
   else
   if(rdma_get_cm_event(_this->cm_channel, &event) )
   {
      _this->errState = -1;
      return ACCEPTRES_ERR;
   }


   // handle event type

   switch(event->event)
   {
      case RDMA_CM_EVENT_CONNECT_REQUEST:
      {
         // got an incoming 'connect request' => check validity of private data and accept/reject

         bool createContextRes;
         struct rdma_conn_param conn_param;
         bool parseCommDestRes;
         IBVCommConfig commCfg;

         struct rdma_cm_id* child_cm_id = event->id;

         //*peerAddrLen = sizeof(struct sockaddr_in);
         //memcpy(peerAddr, &child_cm_id->route.addr.dst_addr, *peerAddrLen);


         // parse private data to get remote dest

         parseCommDestRes = __IBVSocket_parseCommDest(
            event->param.conn.private_data, event->param.conn.private_data_len, &childRemoteDest);
         if(!parseCommDestRes)
         { // bad private data => reject connection
            LOG(SOCKLIB, WARNING, "Bad private data received.",
                  ("len", event->param.conn.private_data_len));

            if(rdma_reject(child_cm_id, NULL, 0) )
               LOG(SOCKLIB, WARNING, "rdma_reject failed.");

            goto ignore;
         }


         // private data (remote dest) okay => create local comm context and socket instance

         // (we use the buffer config as suggested by the connecting peer)
         commCfg.bufNum = childRemoteDest->recvBufNum;
         commCfg.bufSize = childRemoteDest->recvBufSize;

         createContextRes = __IBVSocket_createCommContext(_this, child_cm_id, &commCfg,
            &childCommContext);
         if(!createContextRes)
         {
            LOG(SOCKLIB, WARNING, "Creation of CommContext failed.");

            if(rdma_reject(child_cm_id, NULL, 0) )
               LOG(SOCKLIB, WARNING, "rdma_reject failed.");

            goto ignore;
         }

         acceptedSock = __IBVSocket_constructFromCommContext(child_cm_id, childCommContext);
         if(!acceptedSock->sockValid)
            goto ignore;


         acceptedSock->remoteDest = childRemoteDest;
         childRemoteDest = NULL;  // would otherwise be destroyed at 'ignore'


         // send accept message (with local destination info)

         __IBVSocket_initCommDest(childCommContext, &acceptedSock->localDest);

         memset(&conn_param, 0, sizeof(conn_param) );
#ifdef BEEGFS_NVFS
         conn_param.responder_resources = RDMA_MAX_RESP_RES;
         conn_param.initiator_depth = RDMA_MAX_INIT_DEPTH;
#else
         conn_param.responder_resources = 1;
         conn_param.initiator_depth = 1;
#endif /* BEEGFS_NVFS */
         conn_param.flow_control = 0;
         conn_param.retry_count = 7; // (3 bits)
         conn_param.rnr_retry_count = 7; // rnr = receiver not ready (3 bits, 7 means infinity)
         conn_param.private_data = &acceptedSock->localDest;
         conn_param.private_data_len = sizeof(acceptedSock->localDest);

        // test point for dropping the connect request
        if(IBVSocket_connectionRejection(_this))
           goto ignore;

         if(rdma_accept(child_cm_id, &conn_param) )
         {
            LOG(SOCKLIB, WARNING, "rdma_accept failed.");

            goto ignore;
         }

         if(!__IBVSocket_initEpollFD(acceptedSock) )
            goto ignore;


         // Note that this code returns ACCEPTRES_IGNORE
         LOG(SOCKLIB, DEBUG, "Connection request on RDMASocket");
         child_cm_id->context = acceptedSock;
         acceptedSock = NULL; // would otherwise be destroyed at 'ignore'

      } break;

      case RDMA_CM_EVENT_ESTABLISHED:
      {
         // received 'established' (this is what we've actually been waiting for!)

         *peerAddrLen = sizeof(struct sockaddr_in);
         memcpy(peerAddr, &event->id->route.addr.dst_addr, *peerAddrLen);

         *outAcceptedSock = (IBVSocket*)event->id->context;

         rdma_ack_cm_event(event);

         #ifdef SYSTEM_HAS_RDMA_MIGRATE_ID__disabled

         // note: this is currently disabled, because:
         // a) rdma_migrate_id always returns "invalid argument"
         // b) we need disconnect events for incoming conns to be handled and the handler must call
         //    rdma_disconnect to enable disconnect detection for the streamlistener

         // note: migration might deadlock if there are any retrieved but not yet ack'ed events
         // for the current channel, so we cannot migrate if this is the case

         // note: the only purpose of migration to a separate channel is that we can do better
         // disconnect detection in waitForCompletion(). so living without the migration is
         // generally not a problem (but disconnect detection might take longer).

         if(_this->delayedCmEventsQ->size() )
         { // events waiting => don't migrate
            LOG(SOCKLIB, WARNING,
                  "Skipping rdma_migrate_id due to waiting events (but we can live without it).");
         }
         else
         { // migrate cm_id from general accept-channel to its own channel
            int migrateRes = rdma_migrate_id(
               (*outAcceptedSock)->cm_id, (*outAcceptedSock)->cm_channel);

            if(migrateRes)
            {
               LOG(SOCKLIB, WARNING, "rdma_migrate_id failed (but we can live without it).",
                     migrateRes, sysErr);
            }
         }

         #endif // SYSTEM_HAS_RDMA_MIGRATE_ID

         return ACCEPTRES_SUCCESS;
      } break;

      case RDMA_CM_EVENT_DISCONNECTED:
      {
         // note: be careful about what we do with the event-socket here, because the socket might
         // already be under destruction in another thread.

         LOG(SOCKLIB, DEBUG, "Disconnect event.");

         // note: the additional disconnect call is required to get the streamlistener event
         // channel (the one of the listen sock) to report the disconnect
         rdma_disconnect(event->id);

      } break;

      case RDMA_CM_EVENT_UNREACHABLE:
      {
         LOG(SOCKLIB, WARNING, "Remote unreachable event while waiting for 'established'.");
         acceptedSock = (IBVSocket*)event->id->context; // will be destroyed at 'ignore'
      } break;

      case RDMA_CM_EVENT_CONNECT_ERROR:
      {
         LOG(SOCKLIB, WARNING, "Connect error event while waiting for 'established'.");
         acceptedSock = (IBVSocket*)event->id->context; // will be destroyed at 'ignore'
      } break;

      case RDMA_CM_EVENT_TIMEWAIT_EXIT:
      { // log only with enabled debug code
         LOG(SOCKLIB, DEBUG, "Ignoring conn manager event RDMA_CM_EVENT_TIMEWAIT_EXIT.");
      } break;

      case RDMA_CM_EVENT_DEVICE_REMOVAL:
      {
         AbstractApp* app = PThread::getCurrentThreadApp();
         const char* devname = "unknown";
         if (event->id && event->id->verbs)
            devname = ibv_get_device_name(event->id->verbs->device);
         LOG(SOCKLIB, ERR, "Device removed", ("device", devname));
         app->handleNetworkInterfaceFailure(std::string(devname));
      } break;

      default:
      { // ignore other events
         // always log
         LOG(SOCKLIB, WARNING, "Ignoring conn manager event.",
            ("event", rdma_event_str(event->event)));
      } break;
   }


   // irrelevant event (irrelevant for the caller)
ignore:
   rdma_ack_cm_event(event);

   SAFE_FREE(childRemoteDest);
   if(acceptedSock)
      IBVSocket_destruct(acceptedSock);

   *outAcceptedSock = NULL;

   return ACCEPTRES_IGNORE;
}

bool IBVSocket_shutdown(IBVSocket* _this)
{
   IBVCommContext* commContext = _this->commContext;


   if(!commContext)
      return true; // this socket has never been connected

   // if object is in errState, then the socket might be in an inconsistent state,
   // therefore further commands (except for disconnect) should not be executed
   if(!_this->errState && commContext->incompleteSend.numAvailable)
   { // wait for all incomplete sends
      int waitRes;

      waitRes = __IBVSocket_waitForTotalSendCompletion(
         _this, commContext->incompleteSend.numAvailable, 0, 0);
      if(waitRes < 0)
      {
         LOG(SOCKLIB, WARNING, "Waiting for incomplete send requests failed.");
         return false;
      }
   }

   __IBVSocket_disconnect(_this);

   return true;
}

/**
 * Continues an incomplete former recv() by returning immediately available data from the
 * corresponding buffer.
 */
ssize_t __IBVSocket_recvContinueIncomplete(IBVSocket* _this, char* buf, size_t bufLen)
{
   IBVCommContext* commContext = _this->commContext;
   int completedOffset = commContext->incompleteRecv.completedOffset;
   size_t availableLen = commContext->incompleteRecv.wc.byte_len - completedOffset;
   size_t bufIndex = commContext->incompleteRecv.wc.wr_id - IBVSOCKET_RECV_WORK_ID_OFFSET;


   if(availableLen <= bufLen)
   { // old data fits completely into buf
      memcpy(buf, &(commContext->recvBufs)[bufIndex][completedOffset], availableLen);

      commContext->incompleteRecv.isAvailable = 0;

      int postRes = __IBVSocket_postRecv(_this, _this->commContext, bufIndex);
      if(unlikely(postRes) )
         goto err_invalidateSock;

      return availableLen;
   }
   else
   { // still too much data for the buf => copy partially
      memcpy(buf, &(commContext->recvBufs)[bufIndex][completedOffset], bufLen);

      commContext->incompleteRecv.completedOffset += bufLen;

      return bufLen;
   }


err_invalidateSock:
   _this->errState = -1;

   return -1;
}


ssize_t IBVSocket_recv(IBVSocket* _this, char* buf, size_t bufLen, int flags)
{
   const int timeoutMS = IBVSOCKET_RECV_TIMEOUT_MS;
   ssize_t recvTRes;

   do
   {
      recvTRes = IBVSocket_recvT(_this, buf, bufLen, flags, timeoutMS);
   } while(recvTRes == -ETIMEDOUT);

   return recvTRes;
}


/**
 * @return number of received bytes on success, 0 on timeout, -1 on error
 */
ssize_t IBVSocket_recvT(IBVSocket* _this, char* buf, size_t bufLen, int flags, int timeoutMS)
{
   IBVCommContext* commContext = _this->commContext;
   struct ibv_wc* wc = &commContext->incompleteRecv.wc;
   int flowControlRes;
   int recvWCRes;

   if(unlikely(_this->errState) )
      return -1;

   // check whether an old buffer has not been fully read yet
   if(!commContext->incompleteRecv.isAvailable)
   { // no partially read data available => recv new buffer

      // check whether we have a pending on-send flow control packet that needs to be received first
      flowControlRes = __IBVSocket_flowControlOnSendWait(_this, timeoutMS);
      if(flowControlRes <= 0)
      {
         if(likely(!flowControlRes) )
            return -ETIMEDOUT; // timeout

         goto err_invalidateSock;
      }

      // recv a new buffer (into the incompleteRecv structure)
      recvWCRes = __IBVSocket_recvWC(_this, timeoutMS, wc);
      if(recvWCRes <= 0)
      {
         if(likely(!recvWCRes) )
            return -ETIMEDOUT; // timeout

         goto err_invalidateSock; // error occurred
      }

      // recvWC was positive => we're guaranteed to have an incompleteRecv buf availabe

      commContext->incompleteRecv.completedOffset = 0;
      commContext->incompleteRecv.isAvailable = 1;
   }

   return __IBVSocket_recvContinueIncomplete(_this, buf, bufLen);


err_invalidateSock:
   _this->errState = -1;

   return -ECOMM;
}

ssize_t IBVSocket_send(IBVSocket* _this, const char* buf, size_t bufLen, int flags)
{
   IBVCommContext* commContext = _this->commContext;
   int flowControlRes;
   size_t currentBufIndex;
   int postRes;
   size_t postedLen = 0;
   int currentPostLen;
   int waitRes;

   if(unlikely(_this->errState) )
      return -1;

   do
   {
      flowControlRes = __IBVSocket_flowControlOnSendWait(_this,
         _this->timeoutCfg.flowSendMS);
      if(unlikely(flowControlRes <= 0) )
         goto err_invalidateSock;

      // note: we only poll for completed sends after we used up all (!) available bufs

      if(commContext->incompleteSend.numAvailable == commContext->commCfg.bufNum)
      { // wait for all (!) incomplete sends
         waitRes = __IBVSocket_waitForTotalSendCompletion(
            _this, commContext->incompleteSend.numAvailable, 0, 0);
         if(waitRes < 0)
            goto err_invalidateSock;

         commContext->incompleteSend.numAvailable = 0;
      }

      currentPostLen = BEEGFS_MIN(bufLen-postedLen, commContext->commCfg.bufSize);
      currentBufIndex = commContext->incompleteSend.numAvailable;

      memcpy( (commContext->sendBufs)[currentBufIndex], &buf[postedLen], currentPostLen);

      commContext->incompleteSend.numAvailable++; /* inc'ed before postSend() for conn checks */

      postRes = __IBVSocket_postSend(_this, currentBufIndex, currentPostLen);
      if(unlikely(postRes) )
      {
         commContext->incompleteSend.numAvailable--;
         goto err_invalidateSock;
      }


      postedLen += currentPostLen;

   } while(postedLen < bufLen);

   return (ssize_t)bufLen;


err_invalidateSock:
   _this->errState = -1;

   return -ECOMM;
}


int __IBVSocket_registerBuf(IBVCommContext* commContext, void* buf, size_t bufLen,
   struct ibv_mr** outMR)
{
   /* note: IB spec says:
      "The consumer is not allowed to assign remote-write or remote-atomic to
      a memory region that has not been assigned local-write." */
   enum ibv_access_flags accessFlags = (enum ibv_access_flags)
      (IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);

   *outMR = ibv_reg_mr(commContext->pd, buf, bufLen, accessFlags);
   if(!*outMR)
   {
      LOG(SOCKLIB, WARNING, "Couldn't allocate MR.");
      return -1;
   }

   return 0;
}


char* __IBVSocket_allocAndRegisterBuf(IBVCommContext* commContext, size_t bufLen,
   struct ibv_mr** outMR)
{
   void* buf;
   int registerRes;

   int allocRes = posix_memalign(&buf, sysconf(_SC_PAGESIZE), bufLen);
   if(allocRes)
   {
      LOG(SOCKLIB, WARNING, "Couldn't allocate work buf.");
      return NULL;
   }

   memset(buf, 0, bufLen);

   registerRes = __IBVSocket_registerBuf(commContext, buf, bufLen, outMR);
   if(registerRes < 0)
   {
      free(buf);
      return NULL;
   }

   return (char*)buf;
}

bool __IBVSocket_createCommContext(IBVSocket* _this, struct rdma_cm_id* cm_id,
   IBVCommConfig* commCfg, IBVCommContext** outCommContext)
{
   IBVCommContext* commContext = NULL;
   int registerControlRes;
   int registerControlResReset;
   struct ibv_qp_init_attr qpInitAttr;
   int createQPRes;
   unsigned i;


   // sanity checks

   if (unlikely(commCfg->bufNum < IBVSOCKET_MIN_BUF_NUM) )
   {
      LOG(SOCKLIB, WARNING, "bufNum too small!",
         ("got", commCfg->bufNum), ("minimum", IBVSOCKET_MIN_BUF_NUM));
      goto err_cleanup;
   }

   if (unlikely(commCfg->bufSize < IBVSOCKET_MIN_BUF_SIZE) ) // sanity check
   {
      LOG(SOCKLIB, WARNING, "bufSize too small!",
         ("got", commCfg->bufSize), ("minimum", IBVSOCKET_MIN_BUF_SIZE));
      goto err_cleanup;
   }

   if (commCfg->bufSize * commCfg->bufNum > IBVSOCKET_MAX_BUF_SIZE_NUM)
   {
      LOG(SOCKLIB, WARNING, "bufSize*bufNum too large!",
         ("got", commCfg->bufSize * commCfg->bufNum),
         ("maximum", IBVSOCKET_MAX_BUF_SIZE_NUM));
      goto err_cleanup;
   }


   commContext = (IBVCommContext*)calloc(1, sizeof(*commContext) );
   if(!commContext)
      goto err_cleanup;

   commContext->context = cm_id->verbs;
   if(!commContext->context)
   {
      LOG(SOCKLIB, WARNING, "Unbound cm_id!!");
      goto err_cleanup;
   }

   commContext->pd = ibv_alloc_pd(commContext->context);
   if(!commContext->pd)
   {
      LOG(SOCKLIB, WARNING, "Couldn't allocate PD.");
      goto err_cleanup;
   }

   // alloc and register buffers...

   commContext->commCfg = *commCfg;

   commContext->recvBuf = __IBVSocket_allocAndRegisterBuf(
         commContext, commCfg->bufSize * commCfg->bufNum, &commContext->recvMR);
   if(!commContext->recvBuf)
   {
      LOG(SOCKLIB, WARNING, "Couldn't prepare recvBuf.");
      goto err_cleanup;
   }

   commContext->recvBufs = (char**)calloc(1, commCfg->bufNum * sizeof(char*) );

   for(i=0; i < commCfg->bufNum; i++)
      commContext->recvBufs[i] = &commContext->recvBuf[i * commCfg->bufSize];


   commContext->sendBuf = __IBVSocket_allocAndRegisterBuf(
         commContext, commCfg->bufSize * commCfg->bufNum, &commContext->sendMR);
   if(!commContext->sendBuf)
   {
      LOG(SOCKLIB, WARNING, "Couldn't prepare sendBuf.");
      goto err_cleanup;
   }

   commContext->sendBufs = (char**)calloc(1, commCfg->bufNum * sizeof(char*) );

   for(i=0; i < commCfg->bufNum; i++)
      commContext->sendBufs[i] = &commContext->sendBuf[i * commCfg->bufSize];


   registerControlRes = __IBVSocket_registerBuf(
      commContext, (char*)&commContext->numUsedSendBufs,
      sizeof(commContext->numUsedSendBufs), &commContext->controlMR);
   if(registerControlRes < 0)
   {
      LOG(SOCKLIB, WARNING, "Couldn't register control memory region.");
      goto err_cleanup;
   }

   registerControlResReset = __IBVSocket_registerBuf(
      commContext, (char*)&commContext->numUsedSendBufsReset,
      sizeof(commContext->numUsedSendBufsReset), &commContext->controlResetMR);
   if(registerControlResReset < 0)
   {
      LOG(SOCKLIB, WARNING, "Couldn't register control memory reset region.");
      goto err_cleanup;
   }

   // init flow control v2 (to avoid long receiver-not-ready timeouts)

   /* note: we use -1 because the last buf might not be read by the user (eg during
      nonblockingRecvCheck) and so it might not be immediately available again. */
   commContext->numReceivedBufsLeft = commCfg->bufNum - 1;
   commContext->numSendBufsLeft = commCfg->bufNum - 1;

   // create completion channel and queues...

   commContext->recvCompChannel = ibv_create_comp_channel(commContext->context);
   if(!commContext->recvCompChannel)
   {
      LOG(SOCKLIB, WARNING, "Couldn't create comp channel.");
      goto err_cleanup;
   }

   commContext->recvCQ = ibv_create_cq(
      commContext->context, commCfg->bufNum, commContext, commContext->recvCompChannel,
      rand()%commContext->context->num_comp_vectors);
   if(!commContext->recvCQ)
   {
      LOG(SOCKLIB, WARNING, "Couldn't create recv CQ.");
      goto err_cleanup;
   }

   // note: 1+commCfg->bufNum here for the RDMA write usedBufs reset work (=> flow/flood control)
   commContext->sendCQ = ibv_create_cq(
      commContext->context, 1+commCfg->bufNum, NULL, NULL, 
      rand()%commContext->context->num_comp_vectors);
   if(!commContext->sendCQ)
   {
      LOG(SOCKLIB, WARNING, "Couldn't create send CQ.");
      goto err_cleanup;
   }

   // note: 1+commCfg->bufNum here for the RDMA write usedBufs reset work
   memset(&qpInitAttr, 0, sizeof(qpInitAttr) );

   qpInitAttr.send_cq = commContext->sendCQ;
   qpInitAttr.recv_cq = commContext->recvCQ;
   qpInitAttr.qp_type = IBV_QPT_RC;
   qpInitAttr.sq_sig_all = 1;
   qpInitAttr.cap.max_send_wr = 1+commCfg->bufNum;
   qpInitAttr.cap.max_recv_wr = commCfg->bufNum;
   qpInitAttr.cap.max_send_sge = 1;
   qpInitAttr.cap.max_recv_sge = 1;
   qpInitAttr.cap.max_inline_data = 0;

   createQPRes = rdma_create_qp(cm_id, commContext->pd, &qpInitAttr);
   if(createQPRes)
   {
      LOG(SOCKLIB, WARNING, "Couldn't create QP.", sysErr);
      goto err_cleanup;
   }

   commContext->qp = cm_id->qp;

   // post initial recv buffers...

   for(i=0; i < commCfg->bufNum; i++)
   {
      if(__IBVSocket_postRecv(_this, commContext, i) )
      {
         LOG(SOCKLIB, WARNING, "Couldn't post recv buffer.", ("index", i));
         goto err_cleanup;
      }
   }

   // prepare event notification...

   // initial event notification request
   if(ibv_req_notify_cq(commContext->recvCQ, 0) )
   {
      LOG(SOCKLIB, WARNING, "Couldn't request CQ notification.");
      goto err_cleanup;
   }

#ifdef BEEGFS_NVFS
   commContext->workerMRs = new MRMap();
   commContext->cqMutex = new Mutex();
   commContext->cqCompletions = new CQMap();
   // RDMA id.  (This variable will increment for each RDMA operation.)
   commContext->wr_id = 1;
#endif /* BEEGFS_NVFS */

   LOG(SOCKLIB, DEBUG, __func__,
      ("_this", StringTk::uint64ToHexStr((uint64_t) _this)),
      ("device", cm_id->verbs->device->name));

   *outCommContext = commContext;
   return true;


   //  error handling

err_cleanup:
   __IBVSocket_cleanupCommContext(cm_id, commContext);

   *outCommContext = NULL;
   return false;
}

void __IBVSocket_cleanupCommContext(struct rdma_cm_id* cm_id, IBVCommContext* commContext)
{
   if(!commContext)
      return;

   if(commContext->qp)
   {
      // see recommendation here: https://www.rdmamojo.com/2012/12/28/ibv_destroy_qp/
      // the qp should be set to error state, so that no more events can be pushed to that queue.

      struct ibv_qp_attr qpAttr;
      qpAttr.qp_state = IBV_QPS_ERR;
      if (ibv_modify_qp(commContext->qp, &qpAttr, IBV_QP_STATE))
      {
          LOG(SOCKLIB, WARNING, "Failed to modify qp IBV_QP_STATE.");
      }
   }

   // ack remaining delayed acks
   if(commContext->recvCQ && commContext->numUnackedRecvCompChannelEvents)
      ibv_ack_cq_events(commContext->recvCQ, commContext->numUnackedRecvCompChannelEvents);

   if(commContext->qp)
   {
      rdma_destroy_qp(cm_id);
   }

   if(commContext->sendCQ)
   {
      if(ibv_destroy_cq(commContext->sendCQ) )
         LOG(SOCKLIB, WARNING, "Failed to destroy sendCQ.");
   }

   if(commContext->recvCQ)
   {
      if(ibv_destroy_cq(commContext->recvCQ) )
         LOG(SOCKLIB, WARNING, "Failed to destroy recvCQ.");
   }

   if(commContext->recvCompChannel)
   {
      if(ibv_destroy_comp_channel(commContext->recvCompChannel) )
         LOG(SOCKLIB, WARNING, "Failed to destroy recvCompChannel.");
   }

   if(commContext->controlMR)
   {
      if(ibv_dereg_mr(commContext->controlMR) )
         LOG(SOCKLIB, WARNING, "Failed to deregister controlMR.");
   }

   if(commContext->controlResetMR)
   {
      if(ibv_dereg_mr(commContext->controlResetMR) )
         LOG(SOCKLIB, WARNING, "Failed to deregister controlResetMR.");
   }

   if(commContext->recvMR)
   {
      if(ibv_dereg_mr(commContext->recvMR) )
         LOG(SOCKLIB, WARNING, "Failed to deregister recvMR.");
   }

   if(commContext->sendMR)
   {
      if(ibv_dereg_mr(commContext->sendMR) )
         LOG(SOCKLIB, WARNING, "Failed to deregister sendMR.");
   }

#ifdef BEEGFS_NVFS
   if (commContext->workerMRs)
   {
      for (auto& iter: *(commContext->workerMRs))
      {
         if(ibv_dereg_mr(iter.second) )
            LOG(SOCKLIB, WARNING, "Failed to deregister workerMR.");
      }
      commContext->workerMRs->clear();
      delete(commContext->workerMRs);
   }

   if (commContext->cqCompletions)
   {
      commContext->cqCompletions->clear();
      delete(commContext->cqCompletions);
   }

   delete(commContext->cqMutex);
#endif /* BEEGFS_NVFS */

   SAFE_FREE(commContext->recvBuf);
   SAFE_FREE(commContext->sendBuf);
   SAFE_FREE(commContext->recvBufs);
   SAFE_FREE(commContext->sendBufs);

   if(commContext->pd)
   {
      if(ibv_dealloc_pd(commContext->pd) )
         LOG(SOCKLIB, WARNING, "Failed to dealloc pd.");
   }

   free(commContext);
}

/**
 * Initializes a (local) IBVCommDest.
 */
void __IBVSocket_initCommDest(IBVCommContext* commContext, IBVCommDest* outDest)
{
   memcpy(outDest->verificationStr, IBVSOCKET_PRIVATEDATA_STR, IBVSOCKET_PRIVATEDATA_STR_LEN);

   outDest->protocolVersion = HOST_TO_LE_64(IBVSOCKET_PRIVATEDATA_PROTOCOL_VER);
   outDest->rkey = HOST_TO_LE_32(commContext->controlMR->rkey);
   outDest->vaddr = HOST_TO_LE_64((uintptr_t)&commContext->numUsedSendBufs);
   outDest->recvBufNum = HOST_TO_LE_32(commContext->commCfg.bufNum);
   outDest->recvBufSize = HOST_TO_LE_32(commContext->commCfg.bufSize);
}

/**
 * Checks and parses a (remote) IBVCommDest.
 *
 * @param buf should usually be the private_data of the connection handshake
 * @param outDest will be alloced (if true is returned) and needs to be free'd by the caller
 * @return true if data is okay, false otherwise
 */
bool __IBVSocket_parseCommDest(const void* buf, size_t bufLen, IBVCommDest** outDest)
{
   IBVCommDest* dest = NULL;

   *outDest = NULL;


   // Note: "bufLen < ..." (and not "!="), because there might be some extra padding
   if(!buf || (bufLen < sizeof(*dest) ) )
   {
      LOG(SOCKLIB, WARNING, "Bad private data size.", bufLen);

      return false;
   }

   dest = (IBVCommDest*)malloc(sizeof(*dest) );
   if(!dest)
      return false;

   memcpy(dest, buf, sizeof(*dest) );

   if(memcmp(dest->verificationStr, IBVSOCKET_PRIVATEDATA_STR, IBVSOCKET_PRIVATEDATA_STR_LEN) != 0 )
      goto err_cleanup;

   dest->protocolVersion = LE_TO_HOST_64(dest->protocolVersion);

   if (dest->protocolVersion != IBVSOCKET_PRIVATEDATA_PROTOCOL_VER)
      goto err_cleanup;

   dest->rkey = LE_TO_HOST_32(dest->rkey);
   dest->vaddr = LE_TO_HOST_64(dest->vaddr);
   dest->recvBufNum = LE_TO_HOST_32(dest->recvBufNum);
   dest->recvBufSize = LE_TO_HOST_32(dest->recvBufSize);

   *outDest = dest;

   return true;


err_cleanup:
   SAFE_FREE(dest);

   return false;
}


/**
 * Append buffer to receive queue.
 *
 * @param commContext passed seperately because it's not the _this->commContext during
 *    accept() of incoming connections
 * @return 0 on success, -1 on error
 */
int __IBVSocket_postRecv(IBVSocket* _this, IBVCommContext* commContext, size_t bufIndex)
{
   struct ibv_sge list;
   struct ibv_recv_wr wr;
   struct ibv_recv_wr* bad_wr;
   int postRes;

   list.addr = (uint64_t)commContext->recvBufs[bufIndex];
   list.length = commContext->commCfg.bufSize;
   list.lkey = commContext->recvMR->lkey;

   wr.next = NULL;
   wr.wr_id = bufIndex + IBVSOCKET_RECV_WORK_ID_OFFSET;
   wr.sg_list = &list;
   wr.num_sge = 1;

   postRes = ibv_post_recv(commContext->qp, &wr, &bad_wr);
   if(unlikely(postRes) )
   {
      LOG(SOCKLIB, WARNING, "ibv_post_recv failed.", postRes, sysErr(postRes));
      return -1;
   }

   return 0;
}

/**
 * Synchronous RDMA write (waits for completion)
 *
 * @return 0 on success, -1 on error
 */
int __IBVSocket_postWrite(IBVSocket* _this, IBVCommDest* remoteDest,
   struct ibv_mr* localMR, char* localBuf, int bufLen)
{
   IBVCommContext* commContext = _this->commContext;
   struct ibv_sge list;
   struct ibv_send_wr wr;
   struct ibv_send_wr *bad_wr;
   int postRes;
   int waitRes;

   list.addr = (uint64_t)localBuf;
   list.length = bufLen;
   list.lkey = localMR->lkey;

   wr.wr.rdma.remote_addr = remoteDest->vaddr;
   wr.wr.rdma.rkey = remoteDest->rkey;

   wr.wr_id      = IBVSOCKET_WRITE_WORK_ID;
   wr.sg_list    = &list;
   wr.num_sge    = 1;
   wr.opcode     = IBV_WR_RDMA_WRITE;
   wr.send_flags = IBV_SEND_SIGNALED;
   wr.next       = NULL;

   postRes = ibv_post_send(commContext->qp, &wr, &bad_wr);
   if(unlikely(postRes) )
   {
      LOG(SOCKLIB, WARNING, "ibv_post_send() failed.", sysErr(postRes));
      return -1;
   }

   waitRes = __IBVSocket_waitForTotalSendCompletion(_this,
      commContext->incompleteSend.numAvailable, 1, 0);
   if(unlikely(waitRes) )
      return -1;

   commContext->incompleteSend.numAvailable = 0;

   return 0;
}

/**
 * Synchronous RDMA read (waits for completion).
 *
 * @return 0 on success, -1 on error
 */
int __IBVSocket_postRead(IBVSocket* _this, IBVCommDest* remoteDest,
   struct ibv_mr* localMR, char* localBuf, int bufLen)
{
   IBVCommContext* commContext = _this->commContext;
   struct ibv_sge list;
   struct ibv_send_wr wr;
   struct ibv_send_wr *bad_wr;
   int postRes;
   int waitRes;

   list.addr = (uint64_t) localBuf;
   list.length = bufLen;
   list.lkey = localMR->lkey;

   wr.wr.rdma.remote_addr = remoteDest->vaddr;
   wr.wr.rdma.rkey = remoteDest->rkey;

   wr.wr_id      = IBVSOCKET_READ_WORK_ID;
   wr.sg_list    = &list;
   wr.num_sge    = 1;
   wr.opcode     = IBV_WR_RDMA_READ;
   wr.send_flags = IBV_SEND_SIGNALED;
   wr.next       = NULL;

   postRes = ibv_post_send(commContext->qp, &wr, &bad_wr);
   if(unlikely(postRes) )
   {
      LOG(SOCKLIB, WARNING, "ibv_post_send() failed.", sysErr(postRes));
      return -1;
   }

   waitRes = __IBVSocket_waitForTotalSendCompletion(_this,
      commContext->incompleteSend.numAvailable, 0, 1);
   if(unlikely(waitRes) )
      return -1;

   commContext->incompleteSend.numAvailable = 0;

   return 0;
}

#ifdef BEEGFS_NVFS
static bool __IBVSocket_getBufferKey(IBVCommContext *commContext, char *buffer, unsigned *key)
{
   struct ibv_mr *mr = NULL;

   MRMap::const_iterator iter = commContext->workerMRs->find(buffer);

   if (iter == commContext->workerMRs->end())
   {
      // It is assumed that buffer came from a Worker and is WORKER_BUFOUT_SIZE.
      // TODO: pass around a Buffer with a length instead of unqualified char*.
      // This cache of ibv_mr will potentially grow to Workers * Targets
      // and the ibv_mr instances hang around until the IBVSocket is destroyed.
      // That is probably something to look into...
      if (unlikely(__IBVSocket_registerBuf(commContext, buffer, WORKER_BUFOUT_SIZE, &mr)))
      {
         LOG(SOCKLIB, WARNING, "ibv_postWrite(): failed to register buffer.");
         return false;
      }

      commContext->workerMRs->insert({buffer, mr});
   }
   else
   {
      mr = iter->second;
   }

   *key = mr->lkey;
   return true;
}

/**
 * Wait for the completion of a specific RDMA operation.
 * @return number of completed elements or -1 in case of an error
 */
static int __IBVSocket_waitForRDMACompletion(IBVCommContext* commContext, uint64_t id)
{
   struct ibv_wc wc[IBVSOCKET_WC_ENTRIES];
   int i = 0;
   int found = 0;
   int status = 0;
   int num_wc = 0;

   /*
    * This function is locked so that we don't get a race condition between two workers
    * looking for completions.
    */
   commContext->cqMutex->lock();
   CQMap::const_iterator iter = commContext->cqCompletions->find(id);

   /*
    * Check to see if we have already found the completion we are looking for.
    */
   if (iter != commContext->cqCompletions->end())
   {
      commContext->cqCompletions->erase(id);
      commContext->cqMutex->unlock();
      return 0;
   }

   /*
    * Continue to poll the CQ until we find the entry in question or we encounter a
    * bad status.
    */
   while (!found && !status)
   {
      num_wc = ibv_poll_cq(commContext->sendCQ, IBVSOCKET_WC_ENTRIES, wc);
      if (num_wc > 0)
      {
         for (i = 0; i < num_wc; i++)
         {
            if (unlikely(wc[i].status != IBV_WC_SUCCESS))
            {
               LOG(SOCKLIB, DEBUG, "Connection error.", wc[i].status);
               status = -1;
               break;
            }

            if ((wc[i].opcode == IBV_WC_RDMA_WRITE) || (wc[i].opcode == IBV_WC_RDMA_READ))
            {
               if (wc[i].wr_id == id)
               {
                  found = 1;
               }
               else
               {
                  commContext->cqCompletions->insert({wc[i].wr_id, wc[i].opcode});
               }
            }
            else if (wc[i].opcode == IBV_WC_SEND)
            {
               if (likely(commContext->incompleteSend.numAvailable))
               {
                  commContext->incompleteSend.numAvailable--;
               }
               else
               {
                  LOG(SOCKLIB, WARNING, "Received bad/unexpected send completion.");
                  status = -1;
                  break;
               }
            }
            else
            {
               LOG(SOCKLIB, WARNING, "Received unexpected CQ opcode.", wc[i].opcode);
               status = -1;
               break;
            }
         }
      }
   }

   commContext->cqMutex->unlock();
   return status;
}

/**
 * Process RDMA requests.
 *
 * @return 0 on success, -1 on error
 */
static int __IBVSocket_postRDMA(IBVSocket* _this, ibv_wr_opcode opcode,
   char* localBuf, int bufLen, unsigned lkey,
   uint64_t remoteBuf, unsigned rkey)
{
   IBVCommContext* commContext = _this->commContext;
   struct ibv_sge list;
   struct ibv_send_wr wr;
   struct ibv_send_wr *bad_wr;
   int postRes;
   int waitRes;

   if (unlikely(lkey == 0))
   {
      if (unlikely(!__IBVSocket_getBufferKey(commContext, localBuf, &lkey)))
      {
         LOG(SOCKLIB, WARNING, "ibv_postRDMA(): no local key.");
         return -1;
      }
   }

   list.addr = (uint64_t) localBuf;
   list.length = bufLen;
   list.lkey = lkey;

   wr.wr_id      = __atomic_fetch_add(&commContext->wr_id, 1, __ATOMIC_SEQ_CST);
   wr.next       = NULL;
   wr.sg_list    = &list;
   wr.num_sge    = 1;
   wr.opcode     = opcode;
   wr.send_flags = IBV_SEND_SIGNALED;
   wr.wr.rdma.remote_addr = remoteBuf;
   wr.wr.rdma.rkey = rkey;

   postRes = ibv_post_send(commContext->qp, &wr, &bad_wr);
   if(unlikely(postRes) )
   {
      LOG(SOCKLIB, WARNING, "ibv_post_send() failed.", sysErr(postRes));
      return -1;
   }

   waitRes = __IBVSocket_waitForRDMACompletion(commContext, wr.wr_id);
   return waitRes;
}

int __IBVSocket_postWrite(IBVSocket* _this, char* localBuf, int bufLen,
   unsigned lkey, uint64_t remoteBuf, unsigned rkey)
{
   return __IBVSocket_postRDMA(_this, IBV_WR_RDMA_WRITE, localBuf, bufLen,
      lkey, remoteBuf, rkey);
}

int __IBVSocket_postRead(IBVSocket* _this, char* localBuf, int bufLen,
   unsigned lkey, uint64_t remoteBuf, unsigned rkey)
{
   return __IBVSocket_postRDMA(_this, IBV_WR_RDMA_READ, localBuf, bufLen,
      lkey, remoteBuf, rkey);
}

ssize_t IBVSocket_read(IBVSocket* _this, const char* buf, size_t bufLen,
   unsigned lkey, const uint64_t rbuf, unsigned rkey)
{
   return __IBVSocket_postRead(_this, (char *)buf, bufLen, lkey, rbuf, rkey);
}

ssize_t IBVSocket_write(IBVSocket* _this, const char* buf, size_t bufLen,
 unsigned lkey, const uint64_t rbuf, unsigned rkey)
{
   return __IBVSocket_postWrite(_this, (char *)buf, bufLen, lkey, rbuf, rkey);
}

#endif /* BEEGFS_NVFS */

/**
 * Note: Contains flow control.
 *
 * @return 0 on success, -1 on error
 */
int __IBVSocket_postSend(IBVSocket* _this, size_t bufIndex, int bufLen)
{
   IBVCommContext* commContext = _this->commContext;
   struct ibv_sge list;
   struct ibv_send_wr wr;
   struct ibv_send_wr *bad_wr;
   int postRes;

   list.addr   = (uint64_t)commContext->sendBufs[bufIndex];
   list.length = bufLen;
   list.lkey   = commContext->sendMR->lkey;

   wr.wr_id      = bufIndex + IBVSOCKET_SEND_WORK_ID_OFFSET;
   wr.next       = NULL;
   wr.sg_list    = &list;
   wr.num_sge    = 1;
   wr.opcode     = IBV_WR_SEND;
   wr.send_flags = IBV_SEND_SIGNALED;

   postRes = ibv_post_send(commContext->qp, &wr, &bad_wr);
   if(unlikely(postRes) )
   {
      LOG(SOCKLIB, WARNING, "ibv_post_send() failed.", sysErr(postRes));
      return -1;
   }

   // flow control
   __IBVSocket_flowControlOnSendUpdateCounters(_this);

   return 0;
}


/**
 * Note: Contains flow control.
 *
 * @return 1 on success, 0 on timeout, -1 on error
 */
int __IBVSocket_recvWC(IBVSocket* _this, int timeoutMS, struct ibv_wc* outWC)
{
   IBVCommContext* commContext = _this->commContext;
   size_t bufIndex;

   int waitRes = __IBVSocket_waitForRecvCompletionEvent(_this, timeoutMS, outWC);
   if(waitRes <= 0)
   { // (note: waitRes==0 can often happen, because we call this with timeoutMS==0)

      if(unlikely(waitRes < 0) )
         LOG(SOCKLIB, DEBUG, "Retrieval of completion event failed.", waitRes);
      else
      if(unlikely(timeoutMS) )
         LOG(SOCKLIB, DEBUG, "Waiting for recv completion timed out.");

      return waitRes;
   }

   // we got something...

   if(unlikely(outWC->status != IBV_WC_SUCCESS) )
   {
      LOG(SOCKLIB, DEBUG, "Connection error.", outWC->status);
      return -1;
   }

   bufIndex = outWC->wr_id - IBVSOCKET_RECV_WORK_ID_OFFSET;

   if(unlikely(bufIndex >= commContext->commCfg.bufNum) )
   {
      LOG(SOCKLIB, WARNING, "Completion for unknown/invalid wr_id.", outWC->wr_id);
      return -1;
   }

   // receive completed

   //printf("%s: Recveived %u bytes.\n", __func__, outWC->byte_len); // debug in

   // flow control

   if(unlikely(__IBVSocket_flowControlOnRecv(_this, timeoutMS) ) )
      return -1;

   return 1;
}

/**
 * Intention: Avoid IB rnr by sending control msg when (almost) all our recv bufs are used up to
 * show that we got our new recv bufs ready.
 *
 * @return 0 on success, -1 on error
 */
int __IBVSocket_flowControlOnRecv(IBVSocket* _this, int timeoutMS)
{
   IBVCommContext* commContext = _this->commContext;

   // we received a packet, so peer has received all of our currently pending data => reset counter
   commContext->numSendBufsLeft = commContext->commCfg.bufNum - 1; /* (see
      createCommContext() for "-1" reason) */

   // send control packet if recv counter expires...

   #ifdef BEEGFS_DEBUG
      if(!commContext->numReceivedBufsLeft)
         LOG(SOCKLIB, WARNING, "BUG: numReceivedBufsLeft underflow!");
   #endif // BEEGFS_DEBUG

   commContext->numReceivedBufsLeft--;

   if(!commContext->numReceivedBufsLeft)
   {
      size_t currentBufIndex;
      int postRes;

      if(commContext->incompleteSend.numAvailable == commContext->commCfg.bufNum)
      { // wait for all (!) incomplete sends

         /* note: it's ok that all send bufs are used up, because it's possible that we do a lot of
            recv without the user sending any data in between (so the bufs were actually used up by
            flow control). */

         int waitRes = __IBVSocket_waitForTotalSendCompletion(
            _this, commContext->incompleteSend.numAvailable, 0, 0);
         if(waitRes < 0)
            return -1;

         commContext->incompleteSend.numAvailable = 0;
      }

      currentBufIndex = commContext->incompleteSend.numAvailable;

      commContext->incompleteSend.numAvailable++; /* inc'ed before postSend() for conn checks */

      postRes = __IBVSocket_postSend(_this, currentBufIndex, IBVSOCKET_FLOWCONTROL_MSG_LEN);
      if(unlikely(postRes) )
      {
         commContext->incompleteSend.numAvailable--;
         return -1;
      }


      // note: numReceivedBufsLeft is reset during postSend() flow control
   }

   return 0;
}

/**
 * Called after sending a packet to update flow control counters.
 *
 * Intention: Avoid IB rnr by waiting for control msg when (almost) all peer bufs are used up.
 *
 * Note: This is only one part of the on-send flow control. The other one is
 * _flowControlOnSendWait().
 */
void __IBVSocket_flowControlOnSendUpdateCounters(IBVSocket* _this)
{
   IBVCommContext* commContext = _this->commContext;

   // we sent a packet, so we received all currently pending data from the peer => reset counter
   commContext->numReceivedBufsLeft = commContext->commCfg.bufNum - 1; /* (see
      createCommContext() for "-1" reason) */

   #ifdef BEEGFS_DEBUG

   if(!commContext->numSendBufsLeft)
      LOG(SOCKLIB, WARNING, "BUG: numSendBufsLeft underflow!");

   #endif

   commContext->numSendBufsLeft--;
}

/**
 * Intention: Avoid IB rnr by waiting for control msg when (almost) all peer bufs are used up.
 *
 * @timeoutMS may be 0 for non-blocking operation, otherwise typically
 * IBVSOCKET_FLOWCONTROL_ONSEND_TIMEOUT_MS
 * @return >0 on success, 0 on timeout (waiting for flow control packet from peer), <0 on error
 */
int __IBVSocket_flowControlOnSendWait(IBVSocket* _this, int timeoutMS)
{
   IBVCommContext* commContext = _this->commContext;

   struct ibv_wc wc;
   int recvRes;
   size_t bufIndex;
   int postRecvRes;

   if(commContext->numSendBufsLeft)
      return 1; // flow control not triggered yet

   recvRes = __IBVSocket_recvWC(_this, timeoutMS, &wc);
   if(recvRes <= 0)
      return recvRes;

   bufIndex = wc.wr_id - IBVSOCKET_RECV_WORK_ID_OFFSET;

   if(unlikely(wc.byte_len != IBVSOCKET_FLOWCONTROL_MSG_LEN) )
   { // error (bad length)
      LOG(SOCKLIB, WARNING, "Received flow control packet length mismatch.", wc.byte_len);
      return -1;
   }

   postRecvRes = __IBVSocket_postRecv(_this, commContext, bufIndex);
   if(postRecvRes)
      return -1;

   // note: numSendBufsLeft is reset during recvWC() (if it actually received a packet)

   return 1;
}


/**
 * @return 1 on available data, 0 on timeout, -1 on error
 */
int __IBVSocket_waitForRecvCompletionEvent(IBVSocket* _this, int timeoutMS, struct ibv_wc* outWC)
{
   /* Note: This will also be called with timeoutMS==0 from nonblockingRecvCheck to remove
    * a potentially outdated event notification. for this reason, we have to check the event
    * channel even if "ibv_poll_cq returns 0" and "timeoutMS==0". */

   IBVCommContext* commContext = _this->commContext;
   struct ibv_cq* ev_cq; // event completion queue
   void* ev_ctx; // event context
   struct epoll_event epollEvent;

   // check quick path (is an event available without waiting?)

   int numImmediateEvents = ibv_poll_cq(commContext->recvCQ, 1, outWC);
   if(unlikely(numImmediateEvents < 0) )
   {
      LOG(SOCKLIB, WARNING, "Poll CQ failed.", numImmediateEvents);
      return -1;
   }
   else
   if(numImmediateEvents > 0)
      return 1;


   // no immediate event available => wait for them...

   for( ; ; ) /* (loop until "wc retrieved" or "timeout" or "error") */
   {
      /* note: we use pollTimeoutMS to check the conn every few secs (otherwise we might
         wait for a very long time in case the other side disconnected silently) */
      int pollTimeoutMS = BEEGFS_MIN(_this->timeoutCfg.pollMS, timeoutMS);

      int epollRes = epoll_wait(_this->epollFD, &epollEvent, 1, pollTimeoutMS);
      if(unlikely(epollRes < 0) )
      {
         if(errno == EINTR)
            continue; // ignore EINTR, because debugger causes it

         LOG(SOCKLIB, WARNING, "Epoll error.", sysErr);
         return -1;
      }

      if(epollRes == 0)
      { // poll timed out

         // Note: we check "timeoutMS != 0" here because we don't want to run the
         //    connCheck each time this method is called from nonblockingRecvCheck
         if(timeoutMS)
         {
            int checkRes = IBVSocket_checkConnection(_this);
            if(checkRes < 0)
               return -1;
         }

         timeoutMS -= pollTimeoutMS;
         if(!timeoutMS)
            return 0;

         continue;
      }

      if(unlikely(_this->cm_channel &&
         (epollEvent.data.fd == _this->cm_channel->fd) ) )
      { // cm event incoming
         struct rdma_cm_event* event = 0;

         if (rdma_get_cm_event(_this->cm_channel, &event) < 0)
         {
            LOG(SOCKLIB, DEBUG, "Disconnected by rdma_get_cm_event error.");

            _this->errState = -1;
            return -1;
         }

         // Note: this code doesn't encounter RDMA_CM_EVENT_DEVICE_REMOVAL
         if(event->event == RDMA_CM_EVENT_DISCONNECTED)
         {
            LOG(SOCKLIB, DEBUG, "Disconnect event received.");

            rdma_ack_cm_event(event);

            _this->errState = -1;
            return -1;
         }
         else
         {
            LOG(SOCKLIB, DEBUG, "Ingoring received event",
                  ("event", rdma_event_str(event->event))); // debug in

            rdma_ack_cm_event(event);

            continue;
         }
      }

      // we received a completion event notification => retrieve the event...

      int getEventRes = ibv_get_cq_event(commContext->recvCompChannel, &ev_cq, &ev_ctx);
      if(unlikely(getEventRes) )
      {
         LOG(SOCKLIB, WARNING, "Failed to get cq_event.");
         return -1;
      }

      if(unlikely(ev_cq != commContext->recvCQ) )
      {
         LOG(SOCKLIB, WARNING, "CQ event for unknown CQ.", ev_cq);
         return -1;
      }

      // request notification for next event

      int reqNotifyRes = ibv_req_notify_cq(commContext->recvCQ, 0);
      if(unlikely(reqNotifyRes) )
      {
         LOG(SOCKLIB, WARNING, "Couldn't request CQ notification.");
         return -1;
      }


      // ack is expensive, so we gather and ack multiple events
      // note: spec says we need this, but current send_bw.c & co don't use ibv_ack_cq_events.

      commContext->numUnackedRecvCompChannelEvents++;
      if(commContext->numUnackedRecvCompChannelEvents == IBVSOCKET_EVENTS_GATHER_NUM)
      { // ack events and reset counter
         ibv_ack_cq_events(commContext->recvCQ, commContext->numUnackedRecvCompChannelEvents);
         commContext->numUnackedRecvCompChannelEvents = 0;
      }


      // query event...

      /* note: ibv_poll_cq() does not necessarily return "!=0" after a received event, because the
         event might be outdated */

      int numEvents = ibv_poll_cq(commContext->recvCQ, 1, outWC);
      if(unlikely(numEvents < 0) )
      {
         LOG(SOCKLIB, WARNING, "Poll CQ failed.", numEvents);
         return -1;
      }
      else
      if(numEvents > 0)
         return 1;

      // we received a notification for an outdated event => wait again in the next round

   } // end of for-loop

}


/**
 * @return number of completed elements or -1 in case of an error
 */
int __IBVSocket_waitForTotalSendCompletion(IBVSocket* _this,
   int numSendElements, int numWriteElements, int numReadElements)
{
   IBVCommContext* commContext = _this->commContext;
   int numElements;
   int i;
   size_t bufIndex;
   struct ibv_wc wc[2];


   do
   {
      numElements = ibv_poll_cq(commContext->sendCQ, 2, wc);
      if(unlikely(numElements < 0) )
      {
         LOG(SOCKLIB, WARNING, "Bad ibv_poll_cq result.", numElements);

         return -1;
      }

      // for each completion element
      for(i=0; i < numElements; i++)
      {
         if(unlikely(wc[i].status != IBV_WC_SUCCESS) )
         {
            LOG(SOCKLIB, DEBUG, "Connection error.", wc[i].status);
            return -1;
         }

         switch(wc[i].opcode)
         {
            case IBV_WC_SEND:
            {
               bufIndex = wc[i].wr_id - IBVSOCKET_SEND_WORK_ID_OFFSET;

               if(unlikely(bufIndex >= commContext->commCfg.bufNum) )
               {
                  LOG(SOCKLIB, WARNING, "Bad send completion wr_id.", wc[i].wr_id);
                  return -1;
               }

               if(likely(numSendElements) )
                  numSendElements--;
               else
               {
                  LOG(SOCKLIB, WARNING, "Received bad/unexpected send completion.");

                  return -1;
               }

            } break;

            case IBV_WC_RDMA_WRITE:
            {
               if(unlikely(wc[i].wr_id != IBVSOCKET_WRITE_WORK_ID) )
               {
                  LOG(SOCKLIB, WARNING, "bad write completion wr_id.", wc[i].wr_id);

                  return -1;
               }

               if(likely(numWriteElements) )
                  numWriteElements--;
               else
               {
                  LOG(SOCKLIB, WARNING, "Received bad/unexpected RDMA write completion.");

                  return -1;
               }
            } break;

            case IBV_WC_RDMA_READ:
            {
               if(unlikely(wc[i].wr_id != IBVSOCKET_READ_WORK_ID) )
               {
                  LOG(SOCKLIB, WARNING, "Bad read completion wr_id.", wc[i].wr_id);

                  return -1;
               }

               if(likely(numReadElements) )
                  numReadElements--;
               else
               {
                  LOG(SOCKLIB, WARNING, "Received bad/unexpected RDMA read completion.");

                  return -1;
               }
            } break;

            default:
            {
               LOG(SOCKLIB, WARNING, "Bad/unexpected completion opcode.", wc[i].opcode);

               return -1;
            } break;

         } // end of switch

      } // end of for-loop

   } while(numSendElements || numWriteElements || numReadElements);

   return 0;
}

/**
 * @return 0 on success, -1 on error
 */
int IBVSocket_checkConnection(IBVSocket* _this)
{
   struct ibv_qp_attr qpAttr;
   struct ibv_qp_init_attr qpInitAttr;
   int qpRes;
   int postRes;
   IBVCommContext* commContext = _this->commContext;

   //printf("%s: querying qp...\n", __func__); // debug in

   // check qp status
   qpRes = ibv_query_qp(commContext->qp, &qpAttr, IBV_QP_STATE, &qpInitAttr);
   if(qpRes || (qpAttr.qp_state == IBV_QPS_ERR) )
   {
      LOG(SOCKLIB, WARNING, "Detected QP error state.");

      _this->errState = -1;
      return -1;
   }

   // note: we read a remote value into the numUsedSendBufsReset field, which is actually
   //    meant for something else, so we need to reset the value afterwards

   //printf("%d:%s: post rdma_read to check connection...\n", __LINE__, __func__); // debug in

   postRes = __IBVSocket_postRead(_this, _this->remoteDest, commContext->controlResetMR,
      (char*)&commContext->numUsedSendBufsReset, sizeof(commContext->numUsedSendBufsReset) );
   if(postRes)
   {
      _this->errState = -1;
      return -1;
   }

   commContext->numUsedSendBufsReset = 0;

   //printf("%d:%s: rdma_read succeeded\n", __LINE__, __func__); // debug in

   return 0;
}

/**
 * @return <0 on error, 0 if recv would block, >0 if recv would not block
 */
ssize_t IBVSocket_nonblockingRecvCheck(IBVSocket* _this)
{
   /* note: this will also be called from the stream listener for false alarm checks, so make
    * sure that we remove an (outdated) event from the channel to mute the false alarm. */

   IBVCommContext* commContext = _this->commContext;
   struct ibv_wc* wc = &commContext->incompleteRecv.wc;
   int flowControlRes;
   int recvRes;

   if(unlikely(_this->errState) )
      return -1;

   if(commContext->incompleteRecv.isAvailable)
      return 1;

   // check whether we have a pending on-send flow control packet that needs to be received first
   flowControlRes = __IBVSocket_flowControlOnSendWait(_this, 0);
   if(unlikely(flowControlRes < 0) )
      goto err_invalidateSock;

   if(!flowControlRes)
      return 0;

   // recv one packet (if available) and add it as incompleteRecv
   // or remove event channel notification otherwise (to avoid endless false alerts)
   recvRes = __IBVSocket_recvWC(_this, 0, wc);
   if(unlikely(recvRes < 0) )
      goto err_invalidateSock;

   if(!recvRes)
      return 0;

   // we got something => prepare to continue later

   commContext->incompleteRecv.completedOffset = 0;
   commContext->incompleteRecv.isAvailable = 1;

   return 1;


err_invalidateSock:
   _this->errState = -1;
   return -1;
}

/**
 * Call this after accept() to find out whether more events are waiting (for which
 * no notification would not be delivered through the file descriptor).
 *
 * @return true if more events are waiting and accept() should be called again
 */
bool IBVSocket_checkDelayedEvents(IBVSocket* _this)
{
   bool retVal = false;
   struct rdma_cm_event* event;

   // check for events in the delay queue
   if (!_this->delayedCmEventsQ->empty())
      return true;


   // Switch channel fd to non-blocking, check for waiting events and switch back to blocking.
   // (Quite inefficient, but we really don't have to care about efficiency in this case.)

   // Note: We do this to avoid race conditions (lost events) before we're waiting for
   // new notifications with poll()

   // change mode of the connection manager channel to non-blocking
   int oldChannelFlags = fcntl(IBVSocket_getConnManagerFD(_this), F_GETFL);

   int setNewFlagsRes = fcntl(
      IBVSocket_getConnManagerFD(_this), F_SETFL, oldChannelFlags | O_NONBLOCK);
   if(setNewFlagsRes < 0)
   {
      LOG(SOCKLIB, WARNING, "Set conn manager channel non-blocking failed.", sysErr);
      return false;
   }

   // (non-blocking) check for new events
   if(rdma_get_cm_event(_this->cm_channel, &event) )
   {
      // non-blocking mode, so we ignore "pseudo-errors" here
   }
   else
   { // incoming event available
      //printf("%d:%s: enqueueing an event (during non-blocking check): %d (%s)\n",
      //   __LINE__, __func__, event->event, rdma_event_str(event->event) ); // debug in

      _this->delayedCmEventsQ->push(event);

      retVal = true;
   }


   // change channel mode back to blocking
   int setOldFlagsRes = fcntl(IBVSocket_getConnManagerFD(_this), F_SETFL, oldChannelFlags);
   if(setOldFlagsRes < 0)
   {
      LOG(SOCKLIB, WARNING, "Set conn manager channel blocking failed.", sysErr);
      return false;
   }


   return retVal;
}

void __IBVSocket_disconnect(IBVSocket* _this)
{
   /* note: we only call rdma_disconnect() here if the socket is not connected to the common
      listen sock event channel to avoid a race condition (because the sock accept method also
      calls rdma_disconnect() in the streamlistener thread. ...but that's ok. we really don't need
      that additional event if we're actively disconnecting the sock. */

   if(_this->cm_channel)
   {
      int disconnectRes = rdma_disconnect(_this->cm_id);
      if(disconnectRes)
      {
         LOG(SOCKLIB, WARNING, "rdma disconnect error.", sysErr);
         return;
      }

      // note: we can't wait for events here, because the disconnect event might
      // be received by the listen socket channel (for accepted sockets with older
      // ofed versions).

      /*
      if(!_this->cm_channel || !waitForEvent)
         return;

      rdma_get_cm_event(_this->cm_channel, &event);
      if(event->event != RDMA_CM_EVENT_DISCONNECTED)
      {
         SyslogLogger::log(LOG_WARNING, "%s: unexpected event during disconnect %d: %s\n",
            __func__, event->event, rdma_event_str(event->event) );
      }

      rdma_ack_cm_event(event);
      */
   }

}

void __IBVSocket_close(IBVSocket* _this)
{
   SAFE_FREE(_this->remoteDest);

   if(_this->delayedCmEventsQ)
   { // ack all queued events
      while (!_this->delayedCmEventsQ->empty())
      {
         struct rdma_cm_event* nextEvent =  _this->delayedCmEventsQ->front();

         rdma_ack_cm_event(nextEvent);

         _this->delayedCmEventsQ->pop();
      }

      delete(_this->delayedCmEventsQ);
   }

   if(_this->commContext)
      __IBVSocket_cleanupCommContext(_this->cm_id, _this->commContext);

   if(_this->cm_id)
      rdma_destroy_id(_this->cm_id);
   if(_this->cm_channel)
      rdma_destroy_event_channel(_this->cm_channel);
}

/**
 * Note: Call this for connected sockets only.
 */
bool __IBVSocket_initEpollFD(IBVSocket* _this)
{
   _this->epollFD = epoll_create(1); // "1" is just a hint (and is actually ignored)
   if(_this->epollFD == -1)
   {
      LOG(SOCKLIB, WARNING, "epoll initialization error.", sysErr);
      return false;
   }

   struct epoll_event epollEvent;

   epollEvent.events = EPOLLIN;
   epollEvent.data.fd = IBVSocket_getRecvCompletionFD(_this);

   // note: we only add the recvCompletionFD here and not commContext->context->async_fd, because
      // accepted sockets don't have their own async event channel (they receive events through
      // their parent's fd)

   int epollAddRes = epoll_ctl(_this->epollFD, EPOLL_CTL_ADD,
      IBVSocket_getRecvCompletionFD(_this), &epollEvent);

   if(epollAddRes == -1)
   {
      LOG(SOCKLIB, WARNING, "Unable to add sock to epoll set.", sysErr);

      close(_this->epollFD);
      _this->epollFD = -1;

      return false;
   }

   if(_this->cm_channel)
   {
      epollEvent.events = EPOLLIN;
      epollEvent.data.fd = _this->cm_channel->fd;

      int epollAddRes = epoll_ctl(_this->epollFD, EPOLL_CTL_ADD,
         _this->cm_channel->fd, &epollEvent);

      if(epollAddRes == -1)
      {
         LOG(SOCKLIB, WARNING, "Unable to add sock to epoll set.", sysErr);

         close(_this->epollFD);
         _this->epollFD = -1;

         return false;
      }
   }

   return true;
}

/**
 * @return pointer to static buffer with human readable string for a wc status code
 */
const char* __IBVSocket_wcStatusStr(int wcStatusCode)
{
   switch(wcStatusCode)
   {
      case IBV_WC_WR_FLUSH_ERR:
         return "work request flush error";
      case IBV_WC_RETRY_EXC_ERR:
         return "retries exceeded error";
      case IBV_WC_RESP_TIMEOUT_ERR:
         return "response timeout error";

      default:
         return "<undefined>";
   }

}

bool IBVSocket_getSockValid(IBVSocket* _this)
{
   return _this->sockValid;
}

int IBVSocket_getRecvCompletionFD(IBVSocket* _this)
{
   IBVCommContext* commContext = _this->commContext;

   return commContext ? commContext->recvCompChannel->fd : (-1);
}

int IBVSocket_getConnManagerFD(IBVSocket* _this)
{
   return _this->cm_channel ? _this->cm_channel->fd : (-1);
}

void IBVSocket_setTypeOfService(IBVSocket* _this, uint8_t typeOfService)
{
   _this->typeOfService = typeOfService;
}

void IBVSocket_setTimeouts(IBVSocket* _this, int connectMS, int flowSendMS, int pollMS)
{
   _this->timeoutCfg.connectMS = connectMS > 0 ? connectMS : IBVSOCKET_CONN_TIMEOUT_MS;
   _this->timeoutCfg.flowSendMS = flowSendMS > 0? flowSendMS : IBVSOCKET_FLOWCONTROL_ONSEND_TIMEOUT_MS;
   _this->timeoutCfg.pollMS = pollMS > 0? pollMS : IBVSOCKET_POLL_TIMEOUT_MS;
   LOG(SOCKLIB, DEBUG, "timeouts", ("connectMS", _this->timeoutCfg.connectMS),
      ("flowSendMS", _this->timeoutCfg.flowSendMS), ("pollMS", _this->timeoutCfg.pollMS));
}

void IBVSocket_setConnectionRejectionRate(IBVSocket* _this, unsigned rate)
{
   _this->connectionRejectionRate = rate;
}

bool IBVSocket_connectionRejection(IBVSocket* _this)
{
   if(_this->connectionRejectionRate)
   {
      ++_this->connectionRejectionCount;
      if((_this->connectionRejectionCount % _this->connectionRejectionRate) != 0)
      {
         LOG(SOCKLIB, WARNING, "dropping connection for testing.",
            _this->connectionRejectionCount,
            _this->connectionRejectionRate);
         return true;
      }
   }

   return false;
}

