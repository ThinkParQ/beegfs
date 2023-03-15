#include <app/config/Config.h>
#include <app/App.h>
#include <common/net/message/control/AuthenticateChannelMsg.h>
#include <common/net/message/control/SetChannelDirectMsg.h>
#include <common/net/message/control/PeerInfoMsg.h>
#include <common/net/sock/RDMASocket.h>
#include <common/threading/Thread.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/NetFilter.h>
#include "Node.h"
#include "NodeConnPool.h"


#define NODECONNPOOL_SHUTDOWN_WAITTIMEMS              100


/**
 * Note: localNicCaps should to be initialized before acquiring a stream.
 *
 * @param parentNode the node object to which this conn pool belongs.
 * @param nicList an internal copy will be created.
 */
void NodeConnPool_init(NodeConnPool* this, struct App* app, struct Node* parentNode,
   unsigned short streamPort, NicAddressList* nicList)
{
   Config* cfg = App_getConfig(app);

   this->app = app;

   Mutex_init(&this->mutex);
   Condition_init(&this->changeCond);

   ConnectionList_init(&this->connList);

   ListTk_cloneNicAddressList(nicList, &this->nicList);

   this->establishedConns = 0;
   this->availableConns = 0;

   this->maxConns = Config_getConnMaxInternodeNum(cfg);
   this->fallbackExpirationSecs = Config_getConnFallbackExpirationSecs(cfg);
   this->maxConcurrentAttempts = Config_getConnMaxConcurrentAttempts(cfg);

   sema_init(&this->connSemaphore, this->maxConcurrentAttempts);

   this->parentNode = parentNode;
   this->streamPort = streamPort;
   memset(&this->localNicCaps, 0, sizeof(this->localNicCaps) );
   memset(&this->stats, 0, sizeof(this->stats) );
   memset(&this->errState, 0, sizeof(this->errState) );

   this->logConnErrors = true;
}

NodeConnPool* NodeConnPool_construct(struct App* app, struct Node* parentNode,
   unsigned short streamPort, NicAddressList* nicList)
{
   NodeConnPool* this = (NodeConnPool*)os_kmalloc(sizeof(*this) );

   NodeConnPool_init(this, app, parentNode, streamPort, nicList);

   return this;
}

void NodeConnPool_uninit(NodeConnPool* this)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "NodeConn (uninit)";

   // close open connections
   if(ConnectionList_length(&this->connList) > 0)
   {
      Logger_logFormatted(log, Log_DEBUG, logContext, "Closing %lu connections...",
         ConnectionList_length(&this->connList) );
   }

   // Note: invalidateStreamSocket changes the original connList
   //    (so we must invalidate the iterator each time)
   while(ConnectionList_length(&this->connList) )
   {
      ConnectionListIter connIter;
      PooledSocket* socket;

      ConnectionListIter_init(&connIter, &this->connList);

      socket = ConnectionListIter_value(&connIter);

      __NodeConnPool_invalidateSpecificStreamSocket(this, (Socket*)socket);
   }

   // delete nicList elems
   ListTk_kfreeNicAddressListElems(&this->nicList);

   // normal clean-up
   NicAddressList_uninit(&this->nicList);
   Mutex_uninit(&this->mutex);
}

void NodeConnPool_destruct(NodeConnPool* this)
{
   NodeConnPool_uninit(this);

   kfree(this);
}

/**
 * Note: Will block if no stream socket is immediately available.
 *
 * @return connected socket; NULL on error or pending signal
 */
Socket* NodeConnPool_acquireStreamSocket(NodeConnPool* this)
{
   return NodeConnPool_acquireStreamSocketEx(this, true);
}

/**
 * note: this initially checks for pending signals and returns NULL immediately if signal pending.
 *
 * @param true to allow waiting if all avaiable conns are currently in use; if waiting is not
 * allowed, there is no difference between all conns in use and connection error (so it is expected
 * that the caller will sooner or later allow waiting to detect conn errors).
 * @return connected socket; NULL on error or pending signal
 */
Socket* NodeConnPool_acquireStreamSocketEx(NodeConnPool* this, bool allowWaiting)
{
   const char* logContext = "NodeConn (acquire stream)";

   Logger* log = App_getLogger(this->app);
   NetFilter* netFilter = App_getNetFilter(this->app);
   NetFilter* tcpOnlyFilter = App_getTcpOnlyFilter(this->app);

   Socket* sock = NULL;
   unsigned short port;
   NicAddressList nicListCopy;
   NicAddressListIter nicIter;
   bool isPrimaryInterface = true; // used to set expiration for non-primary interfaces;
      // "primary" means: first interface in the list that is supported by client and server

   if (unlikely(fatal_signal_pending(current)))
      return NULL; // no need to try if the process will terminate soon anyway

   Mutex_lock(&this->mutex); // L O C K

   if(!this->availableConns && (this->establishedConns == this->maxConns) )
   { // wait for a conn to become available (or disconnected)

      if(!allowWaiting)
      { // all conns in use and waiting not allowed => exit
         Mutex_unlock(&this->mutex); // U N L O C K
         return NULL;
      }

      while(!this->availableConns && (this->establishedConns == this->maxConns) )
      { // sleep interruptbile
         cond_wait_res_t waitRes = Condition_waitKillable(&this->changeCond, &this->mutex);
         if(unlikely(waitRes == COND_WAIT_SIGNAL) )
         { // interrupted by signal
            Mutex_unlock(&this->mutex); // U N L O C K
            return NULL;
         }
      }
   }


   if(likely(this->availableConns) )
   { // established connection available => grab it
      PooledSocket* pooledSock;

      ConnectionListIter connIter;
      ConnectionListIter_init(&connIter, &this->connList);

      while(!PooledSocket_isAvailable(ConnectionListIter_value(&connIter) ) )
         ConnectionListIter_next(&connIter);

      pooledSock = ConnectionListIter_value(&connIter);
      PooledSocket_setAvailable(pooledSock, false);
      PooledSocket_setHasActivity(pooledSock);

      this->availableConns--;

      Mutex_unlock(&this->mutex); // U N L O C K

      return (Socket*)pooledSock;
   }


   // no conn available, but maxConns not reached yet => establish a new conn

   port = this->streamPort;
   ListTk_cloneNicAddressList(&this->nicList, &nicListCopy);

   this->establishedConns++;

   Mutex_unlock(&this->mutex); // U N L O C K

   if (this->maxConcurrentAttempts > 0)
   {
      if (down_killable(&this->connSemaphore))
         return NULL;
   }

   // walk over all available NICs, create the corresponding socket and try to connect

   NicAddressListIter_init(&nicIter, &nicListCopy);

   for( ; !NicAddressListIter_end(&nicIter); NicAddressListIter_next(&nicIter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&nicIter);
      const char* nodeTypeStr = Node_getNodeTypeStr(this->parentNode);
      char* endpointStr;
      bool connectRes;

      if(!NetFilter_isAllowed(netFilter, nicAddr->ipAddr) )
         continue;

      if( (nicAddr->nicType != NICADDRTYPE_STANDARD) &&
         NetFilter_isContained(tcpOnlyFilter, nicAddr->ipAddr) )
         continue;

      endpointStr = SocketTk_endpointAddrToString(&nicAddr->ipAddr, port);

      switch(nicAddr->nicType)
      {
         case NICADDRTYPE_RDMA:
         { // RDMA
            if(!this->localNicCaps.supportsRDMA)
               goto continue_clean_endpointStr;

            Logger_logTopFormatted(log, LogTopic_CONN, Log_DEBUG, logContext,
               "Establishing new RDMA connection to: %s@%s", nodeTypeStr, endpointStr);
            sock = (Socket*)RDMASocket_construct();
         } break;
         case NICADDRTYPE_SDP:
         { // SDP
            if(!this->localNicCaps.supportsSDP)
               goto continue_clean_endpointStr;

            Logger_logTopFormatted(log, LogTopic_CONN, Log_DEBUG, logContext,
               "Establishing new SDP connection to: %s@%s", nodeTypeStr, endpointStr);
            sock = (Socket*)StandardSocket_constructSDP();
         } break;
         case NICADDRTYPE_STANDARD:
         { // TCP
            Logger_logTopFormatted(log, LogTopic_CONN, Log_DEBUG, logContext,
               "Establishing new TCP connection to: %s@%s", nodeTypeStr, endpointStr);
            sock = (Socket*)StandardSocket_constructTCP();
         } break;

         default:
         { // unknown
            if(this->logConnErrors)
               Logger_logTopFormatted(log, LogTopic_CONN, Log_WARNING, logContext,
                  "Skipping unknown connection type to: %s@%s", nodeTypeStr, endpointStr);
            goto continue_clean_endpointStr;
         }
      } // end of switch

      // check whether the socket was successfully initialized
      if(!sock)
      { // socket initialization failed
         if(this->logConnErrors)
            Logger_logFormatted(log, Log_WARNING, logContext, "Socket initialization failed: %s@%s",
               nodeTypeStr, endpointStr);

         goto continue_clean_endpointStr;
      }

      // the actual connection attempt
      __NodeConnPool_applySocketOptionsPreConnect(this, sock);

      connectRes = sock->ops->connectByIP(sock, &nicAddr->ipAddr, port);

      if(connectRes)
      { // connected
         struct in_addr peerIP = Socket_getPeerIP(sock);
         NicAddrType_t sockType = Socket_getSockType(sock);

         if(__NodeConnPool_shouldPrintConnectedLogMsg(this, peerIP, sockType) )
         {
            const char* protocolStr = NIC_nicTypeToString(sockType);
            const char* fallbackStr = isPrimaryInterface ? "" : "; fallback route";

            Logger_logTopFormatted(log, LogTopic_CONN, Log_NOTICE, logContext,
               "Connected: %s@%s (protocol: %s%s)",
               nodeTypeStr, endpointStr, protocolStr, fallbackStr);
         }

         __NodeConnPool_applySocketOptionsConnected(this, (Socket*)sock);

         if(!isPrimaryInterface) // non-primary => set expiration counter
         {
            PooledSocket* psock = (PooledSocket*) sock;

            PooledSocket_setExpireTimeStart(psock);
            /* connection establishment can be interrupted for rdma, not for tcp. if we were
             * interrupted during rdma setup and connected via tcp instead, we should close that
             * tco connection as soon as possible. */
            psock->closeOnRelease = signal_pending(current);
         }

         kfree(endpointStr);

         break;
      }
      else
      { // not connected
         if(this->logConnErrors)
         {
            struct in_addr peerIP = Socket_getPeerIP(sock);
            NicAddrType_t sockType = Socket_getSockType(sock);

            if(__NodeConnPool_shouldPrintConnectFailedLogMsg(this, peerIP, sockType) )
            {
               Logger_logTopFormatted(log, LogTopic_CONN, Log_NOTICE, logContext,
                  "Connect failed: %s@%s (protocol: %s)",
                  nodeTypeStr, endpointStr, NIC_nicTypeToString(sockType) );
            }
         }

         // the next interface is definitely non-primary
         isPrimaryInterface = false;

         Socket_virtualDestruct(sock);
      }


   continue_clean_endpointStr:
      kfree(endpointStr);

      if (fatal_signal_pending(current))
         break;
   }

   if (this->maxConcurrentAttempts > 0)
   {
      up(&this->connSemaphore);
   }


   Mutex_lock(&this->mutex); // L O C K

   if(!NicAddressListIter_end(&nicIter) && !fatal_signal_pending(current))
   { // success => add to list (as unavailable) and update stats
      PooledSocket* pooledSock = (PooledSocket*)sock;

      ConnectionList_append(&this->connList, pooledSock);
      __NodeConnPool_statsAddNic(this, PooledSocket_getNicType(pooledSock) );

      __NodeConnPool_setConnSuccess(this, Socket_getPeerIP(sock), Socket_getSockType(sock) );
   }
   else
   { // absolutely unable to connect
      sock = NULL;
      this->establishedConns--;

      // connect may be interrupted by signals. return no connection without setting up alternate
      // routes or other error handling in that case.
      if (!fatal_signal_pending(current))
      {
         if(this->logConnErrors && !__NodeConnPool_getWasLastTimeCompleteFail(this) )
         {
            const char* nodeIDWithTypeStr = Node_getNodeIDWithTypeStr(this->parentNode);

            Logger_logTopFormatted(log, LogTopic_CONN, Log_CRITICAL, logContext,
               "Connect failed on all available routes: %s", nodeIDWithTypeStr);
         }

         __NodeConnPool_setCompleteFail(this);
      }

      // we are not using this connection => notify next waiter
      Condition_signal(&this->changeCond);
   }

   ListTk_kfreeNicAddressListElems(&nicListCopy);
   NicAddressList_uninit(&nicListCopy);

   Mutex_unlock(&this->mutex); // U N L O C K

   return sock;
}

void NodeConnPool_releaseStreamSocket(NodeConnPool* this, Socket* sock)
{
   PooledSocket* pooledSock = (PooledSocket*)sock;

   // test whether this socket has expired

   if(unlikely(PooledSocket_getHasExpired(pooledSock, this->fallbackExpirationSecs) ||
            pooledSock->closeOnRelease))
   { // this socket just expired => invalidate it
      __NodeConnPool_invalidateSpecificStreamSocket(this, sock);
      return;
   }

   // mark the socket as available

   Mutex_lock(&this->mutex);

   this->availableConns++;

   PooledSocket_setAvailable(pooledSock, true);

   Condition_signal(&this->changeCond);

   Mutex_unlock(&this->mutex);
}

void NodeConnPool_invalidateStreamSocket(NodeConnPool* this, Socket* sock)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "NodeConn (invalidate stream)";

   unsigned numInvalidated;

   /* note: we use invalidateAvailableStreams first here, because we want to keep other threads
      from trying to acquire an available socket immediately - and the parameter socket isn't
      availabe anyways. (we don't want to acquire a mutex here that would block the whole pool
      while we're just waiting for the sock-close response messages) */


   numInvalidated = __NodeConnPool_invalidateAvailableStreams(this, false, false);

   Logger_logTopFormatted(log, LogTopic_CONN, 4, logContext,
      "Invalidated %u pooled connections. (1 more invalidation pending...)", numInvalidated);

   __NodeConnPool_invalidateSpecificStreamSocket(this, sock);
}

void __NodeConnPool_invalidateSpecificStreamSocket(NodeConnPool* this, Socket* sock)
{
   const char* logContext = "NodeConn (invalidate stream)";
   Logger* log = App_getLogger(this->app);

   bool sockValid = true;
   ConnectionListIter connIter;
   PooledSocket* pooledSock = (PooledSocket*)sock;
   bool shutdownRes;


   Mutex_lock(&this->mutex); // L O C K

   ConnectionListIter_init(&connIter, &this->connList);

   for( ; !ConnectionListIter_end(&connIter); ConnectionListIter_next(&connIter) )
   {
      Socket* listSock = (Socket*)ConnectionListIter_value(&connIter);
      if(listSock == sock)
         break;
   }

   if(ConnectionListIter_end(&connIter) )
   { // socket not in the list
      Logger_logErrFormatted(log, logContext,
         "Tried to remove a socket that was not found in the pool: %s", Socket_getPeername(sock) );
      sockValid = false;
   }
   else
   {
      this->establishedConns--;

      ConnectionListIter_remove(&connIter);
      __NodeConnPool_statsRemoveNic(this, PooledSocket_getNicType(pooledSock) );

      Condition_signal(&this->changeCond);
   }

   Mutex_unlock(&this->mutex); // U N L O C K

   if(!sockValid)
      return;


   shutdownRes = sock->ops->shutdownAndRecvDisconnect(sock, NODECONNPOOL_SHUTDOWN_WAITTIMEMS);
   if(shutdownRes)
   {
      Logger_logTopFormatted(log, LogTopic_CONN, Log_DEBUG, logContext, "Disconnected: %s@%s",
         Node_getNodeTypeStr(this->parentNode), Socket_getPeername(sock) );
   }
   else
   {
      Logger_logTopFormatted(log, LogTopic_CONN, Log_DEBUG, logContext, "Hard disconnect: %s@%s",
         Node_getNodeTypeStr(this->parentNode), Socket_getPeername(sock) );
   }

   Socket_virtualDestruct(sock);
}

/**
 * Invalidate (disconnect) connections that are currently not acquired.
 *
 * @param idleStreamsOnly invalidate only conns that are marked as idle (ie don't have the activity
 * flag set).
 * @param closeOnRelease if true set every PooledSocket's closeOnRelease flag
 * @return number of invalidated streams
 */
unsigned __NodeConnPool_invalidateAvailableStreams(NodeConnPool* this, bool idleStreamsOnly,
   bool closeOnRelease)
{
   /* note: we have TWO STAGES here, because we don't want to hold the mutex and block everything
      while we're waiting for the conns to be dropped. */

   unsigned numInvalidated = 0; // retVal
   ConnectionListIter connIter;
   ConnectionListIter availableIter;

   ConnectionList availableConnsList;
   ConnectionList_init(&availableConnsList);

   Mutex_lock(&this->mutex); // L O C K

   // STAGE 1: grab all sockets that should be disconnected

   ConnectionListIter_init(&connIter, &this->connList);

   for( ; !ConnectionListIter_end(&connIter); ConnectionListIter_next(&connIter) )
   {
      PooledSocket* sock = ConnectionListIter_value(&connIter);

      if(closeOnRelease)
         sock->closeOnRelease = true;

      if(!PooledSocket_isAvailable(sock) )
         continue; // this one is currently in use

      if(idleStreamsOnly && PooledSocket_getHasActivity(sock) )
         continue; // idle-only requested and this one was not idle

      PooledSocket_setAvailable(sock, false);
      this->availableConns--;
      ConnectionList_append(&availableConnsList, sock);

      numInvalidated++;
   }

   Mutex_unlock(&this->mutex); // U N L O C K


   // STAGE 2: invalidate all grabbed sockets

   ConnectionListIter_init(&availableIter, &availableConnsList);

   for( ; !ConnectionListIter_end(&availableIter); ConnectionListIter_next(&availableIter) )
   {
      PooledSocket* sock = ConnectionListIter_value(&availableIter);
      __NodeConnPool_invalidateSpecificStreamSocket(this, (Socket*)sock);
   }

   ConnectionList_uninit(&availableConnsList);

   return numInvalidated;
}

/**
 * Disconnect all established (available) connections.
 *
 * @return number of disconnected streams
 */
unsigned NodeConnPool_disconnectAvailableStreams(NodeConnPool* this)
{
   unsigned numInvalidated;

   numInvalidated = __NodeConnPool_invalidateAvailableStreams(this, false, false);

   return numInvalidated;
}

/**
 * Note: There is no locking around dropping and resetting afterwards, so there should only be one
 * thread which calls this function (=> InternodeSyncer).
 *
 * @return number of disconnected streams
 */
unsigned NodeConnPool_disconnectAndResetIdleStreams(NodeConnPool* this)
{
   unsigned numInvalidated;

   numInvalidated = __NodeConnPool_invalidateAvailableStreams(this, true, false);

   __NodeConnPool_resetStreamsIdleFlag(this);

   return numInvalidated;
}

/**
 * Resets the activity flag of all available connections to mark them as idle.
 */
void __NodeConnPool_resetStreamsIdleFlag(NodeConnPool* this)
{
   ConnectionListIter connIter;

   Mutex_lock(&this->mutex); // L O C K

   ConnectionListIter_init(&connIter, &this->connList);

   for( ; !ConnectionListIter_end(&connIter); ConnectionListIter_next(&connIter) )
   {
      PooledSocket* sock = ConnectionListIter_value(&connIter);

      if(!PooledSocket_isAvailable(sock) )
         continue; // this one is currently in use

      PooledSocket_resetHasActivity(sock);
   }

   Mutex_unlock(&this->mutex); // U N L O C K
}

bool __NodeConnPool_applySocketOptionsPreConnect(NodeConnPool* this, Socket* sock)
{
   Config* cfg = App_getConfig(this->app);

   NicAddrType_t sockType = Socket_getSockType(sock);

   if(sockType == NICADDRTYPE_STANDARD)
   {
      StandardSocket* stdSock = (StandardSocket*)sock;
      int bufSize = Config_getConnRDMABufNum(cfg) * Config_getConnRDMABufSize(cfg); /* we're just
         re-using the rdma buffer settings here. should later be changed to separate settings */

      StandardSocket_setSoRcvBuf(stdSock, bufSize);
   }
   else
   if(sockType == NICADDRTYPE_RDMA)
   {
      RDMASocket* rdmaSock = (RDMASocket*)sock;
      RDMASocket_setBuffers(rdmaSock, Config_getConnRDMABufNum(cfg),
         Config_getConnRDMABufSize(cfg) );
      RDMASocket_setTimeouts(rdmaSock, Config_getConnRDMATimeoutConnect(cfg),
         Config_getConnRDMATimeoutCompletion(cfg), Config_getConnRDMATimeoutFlowSend(cfg),
         Config_getConnRDMATimeoutFlowRecv(cfg), Config_getConnRDMATimeoutPoll(cfg));
      RDMASocket_setTypeOfService(rdmaSock, Config_getConnRDMATypeOfService(cfg));
      RDMASocket_setConnectionFailureStatus(rdmaSock, Config_getRemapConnectionFailureStatus(cfg));
   }

   return true;
}


/**
 * Apply socket tweaks and send channel control messages.
 */
bool __NodeConnPool_applySocketOptionsConnected(NodeConnPool* this, Socket* sock)
{
   const char* logContext = "NodeConn (apply socket options)";
   Logger* log = App_getLogger(this->app);
   Config* cfg = App_getConfig(this->app);
   uint64_t authHash = Config_getConnAuthHash(cfg);

   bool allSuccessful = true;

   NicAddrType_t sockType = Socket_getSockType(sock);

   // apply general socket options
   if( (sockType == NICADDRTYPE_STANDARD) ||
       (sockType == NICADDRTYPE_SDP) )
   {
      StandardSocket* standardSock = (StandardSocket*)sock;
      bool corkRes;
      bool noDelayRes;
      bool keepAliveRes;

      corkRes = StandardSocket_setTcpCork(standardSock, false);
      if(!corkRes)
      {
         Logger_log(log, Log_NOTICE, logContext, "Failed to disable TcpCork");
         allSuccessful = false;
      }

      noDelayRes = StandardSocket_setTcpNoDelay(standardSock, true);
      if(!noDelayRes)
      {
         Logger_log(log, Log_NOTICE, logContext, "Failed to enable TcpNoDelay");
         allSuccessful = false;
      }

      keepAliveRes = StandardSocket_setSoKeepAlive(standardSock, true);
      if(!keepAliveRes)
      {
         Logger_log(log, Log_NOTICE, logContext, "Failed to enable SoKeepAlive");
         allSuccessful = false;
      }
   }

   // send control messages

   if(authHash)
   { // authenticate channel
      char* sendBuf;
      size_t sendBufLen;
      AuthenticateChannelMsg authMsg;
      size_t sendRes;

      AuthenticateChannelMsg_initFromValue(&authMsg, authHash);
      sendBufLen = NetMessage_getMsgLength( (NetMessage*)&authMsg);
      sendBuf = (char*)os_kmalloc(sendBufLen);

      NetMessage_serialize( (NetMessage*)&authMsg, sendBuf, sendBufLen);
      sendRes = Socket_send(sock, sendBuf, sendBufLen, 0);
      if(sendRes <= 0)
      {
         Logger_log(log, Log_WARNING, logContext, "Failed to send authentication");
         allSuccessful = false;
      }

      kfree(sendBuf);
   }

   { // make channel indirect
      char* sendBuf;
      size_t sendBufLen;
      SetChannelDirectMsg directMsg;
      size_t sendRes;

      SetChannelDirectMsg_initFromValue(&directMsg, 0);
      sendBufLen = NetMessage_getMsgLength( (NetMessage*)&directMsg);
      sendBuf = (char*)os_kmalloc(sendBufLen);

      NetMessage_serialize( (NetMessage*)&directMsg, sendBuf, sendBufLen);
      sendRes = Socket_send(sock, sendBuf, sendBufLen, 0);
      if(sendRes <= 0)
      {
         Logger_log(log, Log_WARNING, logContext, "Failed to set channel to indirect mode");
         allSuccessful = false;
      }

      kfree(sendBuf);
   }

   {
      char* sendBuf;
      size_t sendBufLen;
      PeerInfoMsg peerInfo;
      size_t sendRes = -1;

      PeerInfoMsg_init(&peerInfo, NODETYPE_Client, this->app->localNode->numID);
      sendBufLen = NetMessage_getMsgLength(&peerInfo.netMessage);
      sendBuf = kmalloc(sendBufLen, GFP_KERNEL);

      if (sendBuf)
      {
         NetMessage_serialize(&peerInfo.netMessage, sendBuf, sendBufLen);
         sendRes = Socket_send(sock, sendBuf, sendBufLen, 0);
      }

      if(sendRes <= 0)
      {
         Logger_log(log, Log_WARNING, logContext, "Failed to transmit local node info");
         allSuccessful = false;
      }

      kfree(sendBuf);
   }

   return allSuccessful;
}

void NodeConnPool_getStats(NodeConnPool* this, NodeConnPoolStats* outStats)
{
   Mutex_lock(&this->mutex);

   *outStats = this->stats;

   Mutex_unlock(&this->mutex);
}

/**
 * Gets the first socket peer name for a connection to the specified nicType.
 * Only works on currently available connections.
 *
 * @outBuf buffer for the peer name string (contains "n/a" if no such connection exists or is
 * currently not available).
 * @param outIsNonPrimary set to true if conn in outBuf has an expiration counter, false otherwise
 * @return number of chars written to buf
 */
unsigned NodeConnPool_getFirstPeerName(NodeConnPool* this, NicAddrType_t nicType, ssize_t outBufLen,
   char* outBuf, bool* outIsNonPrimary)
{
   unsigned numWritten = 0; // return value
   ConnectionListIter connIter;
   bool foundMatch = false;

   Mutex_lock(&this->mutex);

   ConnectionListIter_init(&connIter, &this->connList);

   for( ; !ConnectionListIter_end(&connIter); ConnectionListIter_next(&connIter) )
   {
      PooledSocket* pooledSock = ConnectionListIter_value(&connIter);
      Socket* sock = (Socket*)pooledSock;

      if(PooledSocket_isAvailable(pooledSock) &&
         (Socket_getSockType(sock) == nicType) )
      { // found a match => print to buf and stop
         numWritten += scnprintf(outBuf, outBufLen, "%s", Socket_getPeername(sock) );

         *outIsNonPrimary = PooledSocket_getHasExpirationTimer(pooledSock);

         foundMatch = true;
         break;
      }
   }

   if(!foundMatch)
   { // print "n/a"
      numWritten += scnprintf(outBuf, outBufLen, "busy");

      *outIsNonPrimary = false;
   }

   Mutex_unlock(&this->mutex);

   return numWritten;
}


/**
 * Increase stats counter for number of established conns (by NIC type).
 */
void __NodeConnPool_statsAddNic(NodeConnPool* this, NicAddrType_t nicType)
{
   switch(nicType)
   {
      case NICADDRTYPE_RDMA:
      {
         (this->stats.numEstablishedRDMA)++;
      } break;

      case NICADDRTYPE_SDP:
      {
         (this->stats.numEstablishedSDP)++;
      } break;

      default:
      {
         (this->stats.numEstablishedStd)++;
      } break;
   }
}

/**
 * Decrease stats counter for number of established conns (by NIC type).
 */
void __NodeConnPool_statsRemoveNic(NodeConnPool* this, NicAddrType_t nicType)
{
   switch(nicType)
   {
      case NICADDRTYPE_RDMA:
      {
         (this->stats.numEstablishedRDMA)--;
      } break;

      case NICADDRTYPE_SDP:
      {
         (this->stats.numEstablishedSDP)--;
      } break;

      default:
      {
         (this->stats.numEstablishedStd)--;
      } break;
   }
}

/**
 * @param streamPort value 0 will be ignored
 * @param nicList will be copied
 * @return true if port has changed
 */
bool NodeConnPool_updateInterfaces(NodeConnPool* this, unsigned short streamPort,
   NicAddressList* nicList)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "NodeConn (update stream port)";
   bool hasChanged = false; // retVal

   Mutex_lock(&this->mutex); // L O C K

   if(streamPort && (streamPort != this->streamPort) )
   {
      this->streamPort = streamPort;
      hasChanged = true;
      Logger_logFormatted(log, Log_NOTICE, logContext,
         "Node %s port has changed", Node_getID(this->parentNode));
   }

   if (!NicAddressList_equals(&this->nicList, nicList))
   { // update nicList (if allocation of new list fails, we just keep the old list)
      NicAddressList newNicList;

      hasChanged = true;
      Logger_logFormatted(log, Log_NOTICE, logContext,
         "Node %s interfaces have changed", Node_getID(this->parentNode));

      ListTk_cloneNicAddressList(nicList, &newNicList);
      ListTk_kfreeNicAddressListElems(&this->nicList);
      NicAddressList_uninit(&this->nicList);

      this->nicList = newNicList;
   }

   Mutex_unlock(&this->mutex); // U N L O C K

   if (unlikely(hasChanged))
   {
      // closeOnRelease is true, all of these sockets need to be invalidated ASAP
      unsigned numInvalidated = __NodeConnPool_invalidateAvailableStreams(this, false, true);
      if(numInvalidated)
      {
         Logger_logFormatted(log, Log_DEBUG, logContext,
            "Invalidated %u pooled connections (due to port/interface change)", numInvalidated);
      }
   }

   return hasChanged;
}

void __NodeConnPool_setCompleteFail(NodeConnPool* this)
{
   this->errState.lastSuccessPeerIP.s_addr = 0;
   this->errState.wasLastTimeCompleteFail = true;
}

void __NodeConnPool_setConnSuccess(NodeConnPool* this, struct in_addr lastSuccessPeerIP,
   NicAddrType_t lastSuccessNicType)
{
   this->errState.lastSuccessPeerIP = lastSuccessPeerIP;
   this->errState.lastSuccessNicType = lastSuccessNicType;

   this->errState.wasLastTimeCompleteFail = false;
}

bool __NodeConnPool_equalsLastSuccess(NodeConnPool* this, struct in_addr lastSuccessPeerIP,
   NicAddrType_t lastSuccessNicType)
{
   return (this->errState.lastSuccessPeerIP.s_addr == lastSuccessPeerIP.s_addr) &&
      (this->errState.lastSuccessNicType == lastSuccessNicType);

}

bool __NodeConnPool_isLastSuccessInitialized(NodeConnPool* this)
{
   return (this->errState.lastSuccessPeerIP.s_addr != 0);
}

bool __NodeConnPool_shouldPrintConnectedLogMsg(NodeConnPool* this, struct in_addr currentPeerIP,
   NicAddrType_t currentNicType)
{
   /* log only if we didn's succeed at all last time, or if we succeeded last time and it was
      with a different IP/NIC pair */

   return this->errState.wasLastTimeCompleteFail ||
      !__NodeConnPool_isLastSuccessInitialized(this) ||
      !__NodeConnPool_equalsLastSuccess(this, currentPeerIP, currentNicType);
}

bool __NodeConnPool_shouldPrintConnectFailedLogMsg(NodeConnPool* this, struct in_addr currentPeerIP,
   NicAddrType_t currentNicType)
{
   /* log only if this is the first connection attempt or if we succeeded last time this this
      IP/NIC pair */

   return (!this->errState.wasLastTimeCompleteFail &&
      (!__NodeConnPool_isLastSuccessInitialized(this) ||
         __NodeConnPool_equalsLastSuccess(this, currentPeerIP, currentNicType) ) );
}
