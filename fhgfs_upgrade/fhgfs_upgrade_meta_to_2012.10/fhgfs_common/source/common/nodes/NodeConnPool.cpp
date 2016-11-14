#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/control/SetChannelDirectMsg.h>
#include <common/net/sock/StandardSocket.h>
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
 * @param nicList an internal copy will be created
 */
NodeConnPool::NodeConnPool(unsigned short streamPort, NicAddressList& nicList)
{
   AbstractApp* app = PThread::getCurrentThreadApp();
   ICommonConfig* cfg = app->getCommonConfig();

   this->nicList = nicList;
   //this->nicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);

   this->establishedConns = 0;
   this->availableConns = 0;

   this->maxConns = cfg->getConnMaxInternodeNum();
   this->nonPrimaryExpirationCount = cfg->getConnNonPrimaryExpiration();
   this->isChannelDirect = true;

   this->app = app;
   this->streamPort = streamPort;
   memset(&localNicCaps, 0, sizeof(localNicCaps) );
}

NodeConnPool::~NodeConnPool()
{
   LogContext log("NodeConn (destruct)");

   if(!connList.empty())
   {
      log.log(4, std::string("Closing ") + StringTk::intToStr(connList.size() ) +
         std::string(" connections...") );
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

   return NULL;
}



void NodeConnPool::releaseStreamSocket(Socket* sock)
{
   PooledSocket* pooledSock = (PooledSocket*)sock;

   // test whether this socket has expired

   if(unlikely(pooledSock->testAndDecExpirationCounter() ) )
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

   invalidateAllAvailableStreams();
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

   log.log(3, std::string("Disconnected: ") + sock->getPeername() );

   delete(sock);
}

void NodeConnPool::invalidateAllAvailableStreams()
{
   LogContext log("NodeConn (invalidate all streams)");

   ConnectionList availableConnsList;

   SafeMutexLock mutexLock(&mutex);

   if(this->availableConns)
      log.log(4, std::string("Closing ") + StringTk::uintToStr(this->availableConns ) +
         std::string(" available connections...") );

   // grab all available sockets
   for(ConnListIter iter = connList.begin(); this->availableConns; iter++)
   {
      PooledSocket* sock = *iter;

      if(sock->isAvailable() )
      {
         sock->setAvailable(false);
         this->availableConns--;
         availableConnsList.push_back(sock);
      }
   }

   mutexLock.unlock();


   // invalidate all grabbed sockets
   for(ConnListIter iter = availableConnsList.begin(); iter != availableConnsList.end(); iter++)
   {
      invalidateSpecificStreamSocket(*iter);
   }
}


void NodeConnPool::applySocketOptionsPreConnect(StandardSocket* sock)
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
      sock->setTcpNoDelay(true);
   }
   catch(SocketException& e)
   {
      log.log(3, "Failed to enable TcpNoDelay");
   }

   try
   {
      sock->setSoKeepAlive(true);
   }
   catch(SocketException& e)
   {
      log.log(3, "Failed to enable SoKeepAlive");
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

void NodeConnPool::makeChannelIndirect(Socket* sock)
{
   SetChannelDirectMsg directMsg(false);
   char* msgBuf = MessagingTk::createMsgBuf(&directMsg);

   try
   {
      sock->sendto(msgBuf, directMsg.getMsgLength(), 0, NULL, 0);
   }
   catch(SocketException& e)
   {
      free(msgBuf);
      throw e;
   }

   free(msgBuf);
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
      invalidateAllAvailableStreams();
}
