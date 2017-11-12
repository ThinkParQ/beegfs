#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/LocalConnWorker.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/PThread.h>
#include <common/nodes/Node.h>
#include "LocalNodeConnPool.h"

#define NODECONNPOOL_SHUTDOWN_WAITTIMEMS  1500

/**
 * @param nicList an internal copy will be created (and sorted)
 */
LocalNodeConnPool::LocalNodeConnPool(NicAddressList& nicList) :
   NodeConnPool(PThread::getCurrentThreadApp()->getCommonConfig()->getConnMgmtdPortTCP(), nicList)
{
   this->nicList = nicList;
   this->nicList.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);
   
   this->establishedConns = 0;
   this->availableConns = 0;
   
   this->numCreatedWorkers = 0;
   
   this->maxConns = PThread::getCurrentThreadApp()->getCommonConfig()->getConnMaxInternodeNum();
}

LocalNodeConnPool::~LocalNodeConnPool()
{
   LogContext log("LocalNodeConnPool::~LocalNodeConnPool");

   LocalConnWorkerList separateConnWorkerList = connWorkerList; // because invalidateStreamSocket
      // changes the original connList
   
   if(separateConnWorkerList.size() > 0)
   {
      log.log(4, std::string("Closing ") + StringTk::intToStr(separateConnWorkerList.size() ) +
         std::string(" connections...") );
   }
   
   for(LocalConnWorkerListIter iter = separateConnWorkerList.begin();
      iter != separateConnWorkerList.end();
      iter++)
   {
      invalidateStreamSocket( (*iter)->getClientEndpoint() );
   }
   
   nicList.clear();
}

/**
 * @param allowWaiting true to allow waiting if all avaiable conns are currently in use
 * @return connected socket or NULL if all conns busy and waiting is not allowed
 * @throw SocketConnectException if all connection attempts fail, SocketException if other
 * connection problem occurs (e.g. during hand-shake)
 */
Socket* LocalNodeConnPool::acquireStreamSocketEx(bool allowWaiting)
{
   LocalConnWorker* worker = NULL;
   
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
      
      LocalConnWorkerListIter iter = connWorkerList.begin();

      while(!(*iter)->isAvailable() )
         iter++;
         
      worker = *iter;
      worker->setAvailable(false);
            
      availableConns--;

      mutexLock.unlock(); // U N L O C K
      
      return worker->getClientEndpoint();
   }

   
   // no conn available, but maxConns not reached yet => establish a new conn

   std::string contextStr = std::string("LocalNodeConn (acquire stream: internal)");
   LogContext log(contextStr);
   
   establishedConns++;
   
   log.log(3, std::string("Establishing new stream connection to: localhost") );
   
   try
   {
      worker = new LocalConnWorker(StringTk::intToStr(++numCreatedWorkers) );
         
      worker->start();
         
      log.log(3, std::string("Connected to localhost.") );
   }
   catch(ComponentInitException& e)
   {
      log.log(3, std::string("Local connect failed. Exception: ") + std::string(e.what() ) );
   }
   
   
   if(worker)
   {
      // success => add to list (as unavailable)
      connWorkerList.push_back(worker);
   }
   else
   {
      // unable to connect
      establishedConns--;
      
      log.logErr("Connection attempt failed. Giving up.");
   }
   
   
   mutexLock.unlock(); // U N L O C K
   
   if(!worker)
      throw SocketConnectException("Local connection attempt failed");
   
   return worker->getClientEndpoint();
}

void LocalNodeConnPool::releaseStreamSocket(Socket* sock)
{
   SafeMutexLock mutexLock(&mutex);
   
   LocalConnWorkerListIter iter = connWorkerList.begin();

   while( ( (*iter)->getClientEndpoint() != sock) && (iter != connWorkerList.end() ) )
      iter++;
      
   if(iter == connWorkerList.end() )
   {
      std::string contextStr = std::string("LocalNodeConn (release stream: ") +
         sock->getPeername() + ")";
      LogContext log(contextStr);

      log.logErr("Tried to release a socket that was not found in the pool");
   }
   else
   {
      availableConns++;
      
      LocalConnWorker* worker = *iter;
      worker->setAvailable(true);

      changeCond.signal();
   }
   
   mutexLock.unlock();

}

void LocalNodeConnPool::invalidateStreamSocket(Socket* sock)
{
   std::string contextStr = std::string("LocalNodeConn (invalidate stream: ") +
      sock->getPeername() + ")";
   LogContext log(contextStr);
   
   bool sockValid = true;
   LocalConnWorker* worker = NULL;
   
   SafeMutexLock mutexLock(&mutex);
   
   LocalConnWorkerListIter iter = connWorkerList.begin();

   while( ( (*iter)->getClientEndpoint() != sock) && (iter != connWorkerList.end() ) )
      iter++;
      
   if(iter == connWorkerList.end() )
   {
      log.logErr("Tried to remove a socket that was not found in the pool");
      sockValid = false;
   }
   else
   {
      establishedConns--;
      
      worker = *iter;

      connWorkerList.erase(iter);
      
      changeCond.signal();
   }
   
   mutexLock.unlock();
   
   if(!sockValid)
      return;
      

   worker->selfTerminate();

   try
   {
      worker->getClientEndpoint()->shutdownAndRecvDisconnect(NODECONNPOOL_SHUTDOWN_WAITTIMEMS);
      
      log.log(3, std::string("Disconnected: ") + sock->getPeername() );
   }
   catch(SocketException& e)
   {
      log.logErr(std::string("Exceptional disconnect: ") +
         sock->getPeername() + std::string(". ") + e.what() );
   }
   
   worker->join();

   delete(worker);

}

