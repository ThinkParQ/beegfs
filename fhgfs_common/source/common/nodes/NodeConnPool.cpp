#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/control/AuthenticateChannelMsg.h>
#include <common/net/message/control/SetChannelDirectMsg.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/PThread.h>
#include <common/toolkit/MessagingTk.h>
#include "Node.h"
#include "NodeConnPool.h"


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
   ICommonConfig* cfg = app->getCommonConfig();

   this->nicList = nicList;
   //this->nicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);
   this->pathNamedSocket = "";

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

NodeConnPool::NodeConnPool(Node& parentNode, std::string namedSocketPath) : parentNode(parentNode)
{
   AbstractApp* app = PThread::getCurrentThreadApp();
   ICommonConfig* cfg = app->getCommonConfig();

   this->pathNamedSocket = namedSocketPath;

   this->establishedConns = 0;
   this->availableConns = 0;

   this->maxConns = cfg->getConnMaxInternodeNum();
   this->fallbackExpirationSecs = cfg->getConnFallbackExpirationSecs();
   this->isChannelDirect = true;

   this->app = app;
   this->streamPort = 0;
   memset(&localNicCaps, 0, sizeof(localNicCaps) );
   memset(&stats, 0, sizeof(stats) );
   memset(&errState, 0, sizeof(errState) );
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

   std::string contextStr = "NodeConn (acquire stream)";
   LogContext log(contextStr);

   SafeMutexLock mutexLock(&mutex); // L O C K

   if(!availableConns && (establishedConns == maxConns) )
   { // wait for a conn to become available (or disconnected)

      if(!allowWaiting)
      { // all conns in use and waiting not allowed => exit
         mutexLock.unlock(); // U N L O C K
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

      mutexLock.unlock(); // U N L O C K

      return sock;
   }


   // no conn available, but maxConns not reached yet => establish a new conn

   if(this->pathNamedSocket.empty() )
   {
      bool isPrimaryInterface = true; // used to set expiration for non-primary interfaces
         // primary means: first interface in the list that is supported by client and server

      port = this->streamPort;
      NicAddressList nicListCopy = this->nicList;

      std::string contextStr = "NodeConn (acquire stream)";
      LogContext log(contextStr);

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

         std::string endpointStr = parentNode.getNodeTypeStr() + "@" +
            Socket::endpointAddrToString(&iter->ipAddr, port);

         try
         {
            switch(iter->nicType)
            {
               case NICADDRTYPE_RDMA:
               { // RDMA
                  if(!localNicCaps.supportsRDMA)
                     continue;

                  log.log(Log_DEBUG, "Establishing new RDMA connection to: " + endpointStr);
                  newRDMASock = new RDMASocket();
                  sock = newRDMASock;
               } break;
               case NICADDRTYPE_SDP:
               { // SDP
                  if(!localNicCaps.supportsSDP)
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

      mutexLock.relock(); // L O C K

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
   }
   else
   {
      bool namedSocketCreated = false;

      NamedSocket* newNamedSocket;

      establishedConns++;

      mutexLock.unlock(); // U N L O C K

      try
      {
         log.log(Log_DEBUG, "Establishing new named socket connection to: localhost:" +
            this->pathNamedSocket);

         newNamedSocket = new NamedSocket(this->pathNamedSocket);
         sock = newNamedSocket;
         namedSocketCreated = true;
      }
      catch(SocketException& e)
      {
         log.log(Log_NOTICE, std::string("Socket initialization failed: localhost:") +
            this->pathNamedSocket + ". Exception: " + std::string(e.what() ) );
         goto cleanup;
      }

      applySocketOptionsPreConnect(newNamedSocket);

      newNamedSocket->connectToPath();

      if(errState.shouldPrintConnectedLogMsg(sock->getPeerIP(), sock->getSockType() ) )
      {
         std::string protocolStr = NetworkInterfaceCard::nicTypeToString(sock->getSockType() );

         log.log(Log_NOTICE, "Connected: localhost:" + this->pathNamedSocket + " "
            "(protocol: " + protocolStr + ")" );
      }

      mutexLock.relock(); // L O C K

      if(namedSocketCreated)
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
            log.log(Log_CRITICAL, "Connect failed to: localhost:" + this->pathNamedSocket);

            errState.setCompleteFail();
         }

         // we are not using this connection => notify next waiter
         changeCond.signal();
      }
   }

   mutexLock.unlock(); // U N L O C K

cleanup:
   if(!sock)
      throw SocketConnectException("Connect failed on all available routes");

   return sock;
}

void NodeConnPool::releaseStreamSocket(Socket* sock)
{
   PooledSocket* pooledSock = (PooledSocket*)sock;

   // test whether this socket has expired

   if(unlikely(pooledSock->getHasExpired(fallbackExpirationSecs) ) )
   { // this socket just expired => invalidate it
      invalidateSpecificStreamSocket(sock);
      return;
   }

   // mark the socket as available

   SafeMutexLock mutexLock(&mutex);

   availableConns++;

   pooledSock->setAvailable(true);

   changeCond.signal();

   mutexLock.unlock();
}

void NodeConnPool::invalidateStreamSocket(Socket* sock)
{
   // note: we use invalidateAllAvailableStreams first here, because we want to keep other threads
   //    from trying to acquire an available socket immediately - and the parameter socket isn't
   //    availabe anyways. (we don't want to acquire a mutex here that would block the whole pool
   //    while we're just waiting for the sock-close response messages)

   invalidateAllAvailableStreams(false);
   invalidateSpecificStreamSocket(sock);
}

void NodeConnPool::invalidateSpecificStreamSocket(Socket* sock)
{
   std::string contextStr = std::string("NodeConn (invalidate stream)");
   LogContext log(contextStr);

   bool sockValid = true;

   SafeMutexLock mutexLock(&mutex);

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

   mutexLock.unlock();

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
      parentNode.getNodeTypeStr() + "@" + sock->getPeername() );

   delete(sock);
}

/**
 * Invalidate (disconnect) connections that are currently not acquired.
 *
 * @param idleStreamsOnly invalidate only conns that are marked as idle (ie don't have the activity
 * flag set).
 * @return number of invalidated streams
 */
unsigned NodeConnPool::invalidateAllAvailableStreams(bool idleStreamsOnly)
{
   /* note: we have TWO STAGES here, because we don't want to hold the mutex and block everything
      while we're waiting for the conns to be dropped. */

   LogContext log("NodeConn (invalidate all streams)");

   unsigned numInvalidated = 0; // retVal
   ConnectionList availableConnsList;

   SafeMutexLock mutexLock(&mutex); // L O C K

   if(this->availableConns)
      LOG_DEBUG_CONTEXT(log, Log_DEBUG,
         "Currently available connections: " + StringTk::uintToStr(this->availableConns) );


   // STAGE 1: grab all sockets that should be disconnected

   for(ConnListIter iter = connList.begin(); iter != connList.end(); iter++)
   {
      PooledSocket* sock = *iter;

      if(!sock->isAvailable() )
         continue;

      if(idleStreamsOnly && sock->getHasActivity() )
         continue; // idle-only requested and this one was not idle

      sock->setAvailable(false);
      this->availableConns--;
      availableConnsList.push_back(sock);

      numInvalidated++;
   }

   mutexLock.unlock(); // U N L O C K


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

   numInvalidated = invalidateAllAvailableStreams(true);

   resetStreamsIdleFlag();

   return numInvalidated;
}

/**
 * Resets the activity flag of all available connections to mark them as idle.
 */
void NodeConnPool::resetStreamsIdleFlag()
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   for(ConnListIter iter = connList.begin(); iter != connList.end(); iter++)
   {
      PooledSocket* sock = *iter;

      if(!sock->isAvailable() )
         continue;

      sock->resetHasActivity();
   }

   mutexLock.unlock(); // U N L O C K
}


void NodeConnPool::applySocketOptionsPreConnect(RDMASocket* sock)
{
   ICommonConfig* cfg = app->getCommonConfig();

   sock->setBuffers(cfg->getConnRDMABufNum(), cfg->getConnRDMABufSize() );
   sock->setTypeOfService(cfg->getConnRDMATypeOfService());
}

void NodeConnPool::applySocketOptionsPreConnect(StandardSocket* sock)
{
   ICommonConfig* cfg = app->getCommonConfig();

   /* note: we're just re-using the rdma buffer settings here. should later be changed to separate
      settings */

   sock->setSoRcvBuf(cfg->getConnRDMABufNum() * cfg->getConnRDMABufSize() );
}

void NodeConnPool::applySocketOptionsPreConnect(NamedSocket* sock)
{
   ICommonConfig* cfg = app->getCommonConfig();

   /* note: we're just re-using the rdma buffer settings here. should later be changed to separate
      settings */

   sock->setSoRcvBuf(cfg->getConnRDMABufNum() * cfg->getConnRDMABufSize() );
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

   // apply special tcp options

   if(sock->getSockDomain() == PF_INET)
   {
      // increase receive buf length
//      {
//         int rcvBufLen = 1048576;
//         socklen_t rcvBufLenSize = sizeof(rcvBufLen);
//         int rcvBufRes = setsockopt(
//            sock->getFD(), SOL_SOCKET, SO_RCVBUF, &rcvBufLen, rcvBufLenSize);
//         if(rcvBufRes)
//            log.log(3, std::string("Failed to set socket receive buffer size. SysErr: ") +
//               System::getErrString() );
//      }



//      {
//         int sndBufLen = 0;
//         socklen_t sndBufLenSize = sizeof(sndBufLen);
//         int sndBufRes = getsockopt(
//            sock->getFD(), SOL_SOCKET, SO_SNDBUF, &sndBufLen, &sndBufLenSize);
//         if(sndBufRes)
//            log.log(3, std::string("Failed to get socket send buffer size. SysErr: ") +
//               System::getErrString() );
//
//         int rcvBufLen = 0;
//         socklen_t rcvBufLenSize = sizeof(rcvBufLen);
//         int rcvBufRes = getsockopt(
//            sock->getFD(), SOL_SOCKET, SO_RCVBUF, &rcvBufLen, &rcvBufLenSize);
//         if(rcvBufRes)
//            log.log(3, std::string("Failed to get socket receive buffer size. SysErr: ") +
//               System::getErrString() );
//
//         log.log(4, std::string("Socket sndbuf/recvbuf: ") + StringTk::intToStr(sndBufLen) + "/" +
//            StringTk::intToStr(rcvBufLen) );
//      }
   }
}

void NodeConnPool::authenticateChannel(Socket* sock)
{
   uint64_t authHash = app->getCommonConfig()->getConnAuthHash();
   AuthenticateChannelMsg authMsg(authHash);
   std::pair<char*, unsigned> msgBuf = MessagingTk::createMsgBuf(&authMsg);

   try
   {
      sock->sendto(msgBuf.first, msgBuf.second, 0, NULL, 0);
   }
   catch(SocketException& e)
   {
      free(msgBuf.first);
      throw;
   }

   free(msgBuf.first);
}

void NodeConnPool::makeChannelIndirect(Socket* sock)
{
   SetChannelDirectMsg directMsg(false);
   std::pair<char*, unsigned> msgBuf = MessagingTk::createMsgBuf(&directMsg);

   try
   {
      sock->sendto(msgBuf.first, msgBuf.second, 0, NULL, 0);
   }
   catch(SocketException& e)
   {
      free(msgBuf.first);
      throw;
   }

   free(msgBuf.first);
}

/**
 * @param streamPort value 0 will be ignored
 * @param nicList will be copied
 */
void NodeConnPool::updateInterfaces(unsigned short streamPort, NicAddressList& nicList)
{
   bool portHasChanged = false; // we only check port, because nicList check would be too
      // inefficient and not worth the effort

   SafeMutexLock mutexLock(&mutex); // L O C K

   if(streamPort && (streamPort != this->streamPort) )
   {
      this->streamPort = streamPort;
      portHasChanged = true;
   }

   this->nicList = nicList;

   mutexLock.unlock(); // U N L O C K

   if(unlikely(portHasChanged) )
      invalidateAllAvailableStreams(false);
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
   bool foundMatch = false;

   SafeMutexLock mutexLock(&mutex); // L O C K

   for(ConnListIter connIter = connList.begin(); connIter != connList.end(); connIter++)
   {
      PooledSocket* sock = *connIter;

      if(sock->isAvailable() && (sock->getSockType() == nicType) )
      { // found a match => store in out string and stop
         *outPeerName = sock->getPeername();

         *outIsNonPrimary = sock->getHasExpirationTimer();

         foundMatch = true;
         break;
      }
   }

   if(!foundMatch)
   { // print "n/a"
      *outPeerName = "busy";

      *outIsNonPrimary = false;
   }

   mutexLock.unlock(); // U N L O C K

   return foundMatch;
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

      case NICADDRTYPE_NAMEDSOCK:
      {
         (stats.numEstablishedNamed)++;
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

      case NICADDRTYPE_NAMEDSOCK:
      {
         (stats.numEstablishedNamed)--;
      } break;

      default:
      {
         (stats.numEstablishedStd)--;
      } break;
   }
}
