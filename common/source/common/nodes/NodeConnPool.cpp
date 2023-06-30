#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/control/AuthenticateChannelMsg.h>
#include <common/net/message/control/SetChannelDirectMsg.h>
#include <common/threading/PThread.h>
#include <common/toolkit/MessagingTk.h>
#include "Node.h"
#include "NodeConnPool.h"

#include <mutex>

#include <boost/lexical_cast.hpp>

#define NODECONNPOOL_SHUTDOWN_WAITTIMEMS              500


/**
 * Note: localNicCaps should to be initialized before acquiring a stream.
 * Note: The opened comm channels are direct by default.
 *
 * @param parentNode the node to which this store belogs (to have nodeType for log messages etc).
 * @param nicList an internal copy will be created.
 */
NodeConnPool::NodeConnPool(Node& parentNode, unsigned short streamPort, const NicAddressList& nicList):
   parentNode(parentNode)
{
   AbstractApp* app = PThread::getCurrentThreadApp();
   auto cfg = app->getCommonConfig();

   this->nicList = nicList;

   this->establishedConns = 0;
   this->availableConns = 0;

   this->maxConns = cfg->getConnMaxInternodeNum();
   this->fallbackExpirationSecs = cfg->getConnFallbackExpirationSecs();
   this->isChannelDirect = true;

   this->app = app;
   this->streamPort = streamPort;
   memset(&localNicCaps, 0, sizeof(localNicCaps) );
   memset(&stats, 0, sizeof(stats) );
   memset(&errState, 0, sizeof(errState) );

}

void NodeConnPool::setLocalNicList(const NicAddressList& localNicList,
   const NicListCapabilities& localNicCaps)
{
   std::unique_lock<Mutex> mutexLock(mutex);
   this->localNicList = localNicList;
   this->localNicCaps = localNicCaps;
   mutexLock.unlock();
   invalidateAllAvailableStreams(false, true);
}

NodeConnPool::~NodeConnPool()
{
   const char* logContext = "NodeConn (destruct)";

   if(!connList.empty() )
   {
      LogContext(logContext).log(Log_DEBUG,
         "Closing " + StringTk::intToStr(connList.size() ) + " connections...");
   }

   while(!connList.empty() )
   {
      ConnListIter iter = connList.begin();
      invalidateSpecificStreamSocket(*iter);
   }

   nicList.clear();
}

/**
 * Note: Will block if no stream socket is immediately available.
 *
 * @return connected socket
 * @throw SocketConnectException if all connection attempts fail, SocketException if other
 * connection problem occurs (e.g. during hand-shake)
 */
Socket* NodeConnPool::acquireStreamSocket()
{
   return acquireStreamSocketEx(true);
}

/**
 * @param true to allow waiting if all avaiable conns are currently in use
 * @return connected socket or NULL if all conns busy and waiting is not allowed
 * @throw SocketConnectException if all connection attempts fail, SocketException if other
 * connection problem occurs (e.g. during hand-shake)
 */
Socket* NodeConnPool::acquireStreamSocketEx(bool allowWaiting)
{
   PooledSocket* sock = NULL;
   unsigned short port;

   LogContext log("NodeConn (acquire stream)");

   std::unique_lock<Mutex> mutexLock(mutex); // L O C K

   if(!availableConns && (establishedConns == maxConns) )
   { // wait for a conn to become available (or disconnected)

      if(!allowWaiting)
      { // all conns in use and waiting not allowed => exit
         return NULL;
      }

      while(!availableConns && (establishedConns == maxConns) )
         changeCond.wait(&mutex);
   }

   if(likely(availableConns) )
   {
      // established connection available => grab it

      ConnListIter iter = connList.begin();

      while(!(*iter)->isAvailable() )
         iter++;

      sock = *iter;
      sock->setAvailable(false);
      sock->setHasActivity();

      availableConns--;

      return sock;
   }


   // no conn available, but maxConns not reached yet => establish a new conn

   bool isPrimaryInterface = true; // used to set expiration for non-primary interfaces
      // primary means: first interface in the list that is supported by client and server

   port = this->streamPort;
   NicAddressList nicListCopy = this->nicList;
   NicListCapabilities localNicCapsCopy = this->localNicCaps;

   establishedConns++;

   mutexLock.unlock(); // U N L O C K

   // walk over all available NICs, create the corresponding socket and try to connect

   NicAddressListIter iter = nicListCopy.begin();
   for( ; iter != nicListCopy.end(); iter++)
   {
      StandardSocket* newStandardSock = NULL; // (to find out whether we need to set the
         // socketOptions without runtime type info)
      RDMASocket* newRDMASock = NULL; // (to find out whether we need to set the
         // socketOptions without runtime type info)

      if(!app->getNetFilter()->isAllowed(iter->ipAddr.s_addr) )
         continue;

      if( (iter->nicType != NICADDRTYPE_STANDARD) &&
         app->getTcpOnlyFilter()->isContained(iter->ipAddr.s_addr) )
         continue;

      std::string endpointStr = boost::lexical_cast<std::string>(parentNode.getNodeType()) + "@" +
         Socket::endpointAddrToString(&iter->ipAddr, port);

      try
      {
         switch(iter->nicType)
         {
            case NICADDRTYPE_RDMA:
            { // RDMA
               if(!localNicCapsCopy.supportsRDMA)
                  continue;

               log.log(Log_DEBUG, "Establishing new RDMA connection to: " + endpointStr);
               newRDMASock = RDMASocket::create().release();
               sock = newRDMASock;
            } break;
            case NICADDRTYPE_SDP:
            { // SDP
               if(!localNicCapsCopy.supportsSDP)
                  continue;

               log.log(Log_DEBUG, "Establishing new SDP connection to: " + endpointStr);
               newStandardSock = new StandardSocket(PF_SDP, SOCK_STREAM);
               sock = newStandardSock;
            } break;
            case NICADDRTYPE_STANDARD:
            { // TCP
               log.log(Log_DEBUG, "Establishing new TCP connection to: " + endpointStr);
               newStandardSock = new StandardSocket(PF_INET, SOCK_STREAM);
               sock = newStandardSock;
            } break;

            default:
            { // unknown
               log.log(Log_WARNING, "Skipping unknown connection type to: " + endpointStr);
               continue;
            }

         } // end of switch

      } // end of try
      catch(SocketException& e)
      {
         log.log(Log_NOTICE, std::string("Socket initialization failed: ") + endpointStr + ". "
            "Exception: " + std::string(e.what() ) );

         continue;
      }

      if (!sock) {
         LOG(GENERAL, ERR, "Failed to open socket for unknown reasons.", endpointStr);
         continue;
      }

      try
      {
         // the actual connection attempt
         if(newRDMASock)
            applySocketOptionsPreConnect(newRDMASock);
         else
         if(newStandardSock)
            applySocketOptionsPreConnect(newStandardSock);

         sock->connect(&iter->ipAddr, port);

         if(errState.shouldPrintConnectedLogMsg(sock->getPeerIP(), sock->getSockType() ) )
         {
            std::string protocolStr = NetworkInterfaceCard::nicTypeToString(
               sock->getSockType() );
            std::string fallbackStr = isPrimaryInterface ? "" : "; fallback route";

            log.log(Log_NOTICE, "Connected: " + endpointStr + " "
               "(protocol: " + protocolStr + fallbackStr + ")" );
         }

         if(newStandardSock)
            applySocketOptionsConnected(newStandardSock);

         if(app->getCommonConfig()->getConnAuthHash() )
            authenticateChannel(sock);

         if(!isChannelDirect)
            makeChannelIndirect(sock);

         if(!isPrimaryInterface) // non-primary => set expiration counter
            sock->setExpireTimeStart();

         break;
      }
      catch(SocketException& e)
      {
         if(errState.shouldPrintConnectFailedLogMsg(sock->getPeerIP(), sock->getSockType() ) )
            log.log(Log_NOTICE, std::string("Connect failed: ") + endpointStr + " "
               "(protocol: " + NetworkInterfaceCard::nicTypeToString(sock->getSockType() ) +
               "); Error: " + std::string(e.what() ) );

         // the next interface is definitely non-primary
         isPrimaryInterface = false;

         delete(sock);
      }
   } // end of connect loop

   mutexLock.lock(); // L O C K

   if(iter != nicListCopy.end() )
   {
      // success => add to list (as unavailable)
      connList.push_back(sock);
      statsAddNic(sock->getSockType() );

      errState.setConnSuccess(sock->getPeerIP(), sock->getSockType() );
   }
   else
   {
      // absolutely unable to connect
      sock = NULL;
      establishedConns--;

      if(!errState.getWasLastTimeCompleteFail() )
      {
         log.log(Log_CRITICAL,
            "Connect failed on all available routes: " + parentNode.getNodeIDWithTypeStr() );

         errState.setCompleteFail();
      }

      // we are not using this connection => notify next waiter
      changeCond.signal();
   }

   if(!sock)
      throw SocketConnectException("Connect failed on all available routes");

   return sock;
}

void NodeConnPool::releaseStreamSocket(Socket* sock)
{
   PooledSocket* pooledSock = (PooledSocket*)sock;

   if(unlikely(pooledSock->getHasExpired(fallbackExpirationSecs) ) ||
      unlikely(pooledSock->isCloseOnRelease() ) )
   {
      // this socket just expired or is set to be closed => invalidate it.
      // note: changeCond is signaled by this call
      invalidateSpecificStreamSocket(sock);
      return;
   }

   // mark the socket as available

   const std::lock_guard<Mutex> lock(mutex);

   availableConns++;

   pooledSock->setAvailable(true);

   changeCond.signal();
}

void NodeConnPool::invalidateStreamSocket(Socket* sock)
{
   // note: we use invalidateAllAvailableStreams first here, because we want to keep other threads
   //    from trying to acquire an available socket immediately - and the parameter socket isn't
   //    availabe anyways. (we don't want to acquire a mutex here that would block the whole pool
   //    while we're just waiting for the sock-close response messages)

   invalidateAllAvailableStreams(false, false);
   invalidateSpecificStreamSocket(sock);
}

void NodeConnPool::invalidateSpecificStreamSocket(Socket* sock)
{
   LogContext log("NodeConn (invalidate stream)");

   bool sockValid = true;

   {
      const std::lock_guard<Mutex> lock(mutex);

      ConnListIter iter = connList.begin();

      while( (*iter != ((PooledSocket*)sock) ) && (iter != connList.end() ) )
         iter++;

      if(unlikely(iter == connList.end() ) )
      {
         log.logErr("Tried to remove a socket that was not found in the pool: " +
            sock->getPeername() );
         sockValid = false;
      }
      else
      { // found the socket in the pool => remove from pool list
         establishedConns--;

         connList.erase(iter);
         statsRemoveNic(sock->getSockType() );

         changeCond.signal();
      }
   }

   if(!sockValid)
      return;


   try
   {
      sock->shutdownAndRecvDisconnect(NODECONNPOOL_SHUTDOWN_WAITTIMEMS);
   }
   catch(SocketException& e)
   {
      // nothing to be done here (we wanted to invalidate the conn anyways)
   }

   log.log(Log_DEBUG, std::string("Disconnected: ") +
      boost::lexical_cast<std::string>(parentNode.getNodeType()) + "@" + sock->getPeername() );

   delete(sock);
}

/**
 * Invalidate (disconnect) connections that are currently not acquired.
 *
 * @param idleStreamsOnly invalidate only conns that are marked as idle (ie don't have the activity
 * flag set).
 * @param closeOnRelease if true, set every PooledSocket's closeOnRelease flag.
 * @return number of invalidated streams
 */
unsigned NodeConnPool::invalidateAllAvailableStreams(bool idleStreamsOnly, bool closeOnRelease)
{
   /* note: we have TWO STAGES here, because we don't want to hold the mutex and block everything
      while we're waiting for the conns to be dropped. */

   LogContext log("NodeConn (invalidate all streams)");

   unsigned numInvalidated = 0; // retVal
   ConnectionList availableConnsList;

   {
      const std::lock_guard<Mutex> lock(mutex);

      if(this->availableConns)
         LOG_DEBUG_CONTEXT(log, Log_DEBUG,
            "Currently available connections: " + StringTk::uintToStr(this->availableConns) );


      // STAGE 1: grab all sockets that should be disconnected

      for(ConnListIter iter = connList.begin(); iter != connList.end(); iter++)
      {
         PooledSocket* sock = *iter;

         if (closeOnRelease )
            sock->setCloseOnRelease(true);

         if(!sock->isAvailable() )
            continue;

         if(idleStreamsOnly && sock->getHasActivity() )
            continue; // idle-only requested and this one was not idle

         sock->setAvailable(false);
         this->availableConns--;
         availableConnsList.push_back(sock);

         numInvalidated++;
      }
   }


   // STAGE 2: invalidate all grabbed sockets

   for(ConnListIter iter = availableConnsList.begin(); iter != availableConnsList.end(); iter++)
   {
      invalidateSpecificStreamSocket(*iter);
   }

   return numInvalidated;
}

/**
 * Note: There is no locking around dropping and resetting afterwards, so there should only be one
 * thread which calls this function (=> InternodeSyncer).
 *
 * @return number of disconnected streams
 */
unsigned NodeConnPool::disconnectAndResetIdleStreams()
{
   unsigned numInvalidated;

   numInvalidated = invalidateAllAvailableStreams(true, false);

   resetStreamsIdleFlag();

   return numInvalidated;
}

/**
 * Resets the activity flag of all available connections to mark them as idle.
 */
void NodeConnPool::resetStreamsIdleFlag()
{
   const std::lock_guard<Mutex> lock(mutex);

   for(ConnListIter iter = connList.begin(); iter != connList.end(); iter++)
   {
      PooledSocket* sock = *iter;

      if(!sock->isAvailable() )
         continue;

      sock->resetHasActivity();
   }
}


void NodeConnPool::applySocketOptionsPreConnect(RDMASocket* sock)
{
   auto cfg = app->getCommonConfig();

   sock->setBuffers(cfg->getConnRDMABufNum(), cfg->getConnRDMABufSize() );
   sock->setTimeouts(cfg->getConnRDMATimeoutConnect(), cfg->getConnRDMATimeoutFlowSend(),
      cfg->getConnRDMATimeoutPoll());
   sock->setTypeOfService(cfg->getConnRDMATypeOfService());
}

void NodeConnPool::applySocketOptionsPreConnect(StandardSocket* sock)
{
   auto cfg = app->getCommonConfig();
   int bufsize = cfg->getConnTCPRcvBufSize();

   if (bufsize > 0)
      sock->setSoRcvBuf(bufsize);
}

void NodeConnPool::applySocketOptionsConnected(StandardSocket* sock)
{
   LogContext log("NodeConn (apply socket options");

   // apply general socket options

   try
   {
      sock->setTcpCork(false);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to disable TcpCork");
   }

   try
   {
      sock->setTcpNoDelay(true);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to enable TcpNoDelay");
   }

   try
   {
      sock->setSoKeepAlive(true);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to enable SoKeepAlive");
   }
}

void NodeConnPool::authenticateChannel(Socket* sock)
{
   uint64_t authHash = app->getCommonConfig()->getConnAuthHash();
   AuthenticateChannelMsg authMsg(authHash);
   const auto msgBuf = MessagingTk::createMsgVec(authMsg);

   sock->sendto(&msgBuf[0], msgBuf.size(), 0, NULL, 0);
}

void NodeConnPool::makeChannelIndirect(Socket* sock)
{
   SetChannelDirectMsg directMsg(false);
   const auto msgBuf = MessagingTk::createMsgVec(directMsg);

   sock->sendto(&msgBuf[0], msgBuf.size(), 0, NULL, 0);
}

/**
 * @param streamPort value 0 will be ignored
 * @param nicList will be copied
 */
bool NodeConnPool::updateInterfaces(unsigned short streamPort, const NicAddressList& nicList)
{
   bool hasChanged = false;
   {
      const std::lock_guard<Mutex> lock(mutex);

      if(streamPort && (streamPort != this->streamPort) )
      {
         LOG(GENERAL, NOTICE, "Node port has changed", ("node", parentNode.getNodeIDWithTypeStr()));
         this->streamPort = streamPort;
         hasChanged = true;
      }

      if (!(this->nicList == nicList))
      {
         LOG(GENERAL, NOTICE, "Node interfaces have changed", ("node", parentNode.getNodeIDWithTypeStr()));
         hasChanged = true;
         this->nicList = nicList;
      }
   }

   if(unlikely(hasChanged) )
   {
      // closeOnRelease is true, all of these sockets need to be invalidated ASAP
      invalidateAllAvailableStreams(false, true);
   }

   return hasChanged;
}

/**
 * Gets the first socket peer name for a connection to the specified nicType.
 * Only works on currently available connections.
 *
 * @outPeerName buffer for the peer name string (contains "n/a" if no such connection exists or is
 * currently not available).
 * @param outIsNonPrimary set to true if conn in outBuf has an expiration counter, false otherwise.
 * @return true if peer name filled in, false if all established conns busy.
 */
bool NodeConnPool::getFirstPeerName(NicAddrType nicType, std::string* outPeerName,
   bool* outIsNonPrimary)
{
   const std::lock_guard<Mutex> lock(mutex);

   for(ConnListIter connIter = connList.begin(); connIter != connList.end(); connIter++)
   {
      PooledSocket* sock = *connIter;

      if(sock->isAvailable() && (sock->getSockType() == nicType) )
      { // found a match => store in out string and stop
         *outPeerName = sock->getPeername();

         *outIsNonPrimary = sock->getHasExpirationTimer();

         return true;
      }
   }

   // print "n/a"
   *outPeerName = "busy";

   *outIsNonPrimary = false;

   return false;
}

/**
 * Increase stats counter for number of established conns (by NIC type).
 */
void NodeConnPool::statsAddNic(NicAddrType nicType)
{
   switch(nicType)
   {
      case NICADDRTYPE_RDMA:
      {
         (stats.numEstablishedRDMA)++;
      } break;

      case NICADDRTYPE_SDP:
      {
         (stats.numEstablishedSDP)++;
      } break;

      default:
      {
         (stats.numEstablishedStd)++;
      } break;
   }
}

/**
 * Decrease stats counter for number of established conns (by NIC type).
 */
void NodeConnPool::statsRemoveNic(NicAddrType nicType)
{
   switch(nicType)
   {
      case NICADDRTYPE_RDMA:
      {
         (stats.numEstablishedRDMA)--;
      } break;

      case NICADDRTYPE_SDP:
      {
         (stats.numEstablishedSDP)--;
      } break;

      default:
      {
         (stats.numEstablishedStd)--;
      } break;
   }
}
