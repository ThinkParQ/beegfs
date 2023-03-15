#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/LocalConnWorker.h>
#include <common/threading/PThread.h>
#include <common/nodes/Node.h>
#include "LocalNodeConnPool.h"

#include <mutex>

#define NODECONNPOOL_SHUTDOWN_WAITTIMEMS  1500

/**
 * @param nicList an internal copy will be created
 */
LocalNodeConnPool::LocalNodeConnPool(Node& parentNode, NicAddressList& nicList) :
   NodeConnPool(parentNode, 0, nicList)
{
   this->nicList = nicList;

   this->establishedConns = 0;
   this->availableConns = 0;

   this->numCreatedWorkers = 0;

   this->maxConns = PThread::getCurrentThreadApp()->getCommonConfig()->getConnMaxInternodeNum();
}

LocalNodeConnPool::~LocalNodeConnPool()
{
   LogContext log("LocalNodeConnPool::~LocalNodeConnPool");

   UnixConnWorkerList separateConnWorkerList = connWorkerList; // because invalidateStreamSocket
      // changes the original connList

   if (!separateConnWorkerList.empty())
   {
      log.log(4, std::string("Closing ") + StringTk::intToStr(separateConnWorkerList.size() ) +
         std::string(" connections...") );
   }

   for(UnixConnWorkerListIter iter = separateConnWorkerList.begin();
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
   UnixConnWorker* worker = NULL;

   {
      const std::lock_guard<Mutex> lock(mutex);

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

         UnixConnWorkerListIter iter = connWorkerList.begin();

         while(!(*iter)->isAvailable() )
            iter++;

         worker = *iter;
         worker->setAvailable(false);

         availableConns--;

         return worker->getClientEndpoint();
      }


      // no conn available, but maxConns not reached yet => establish a new conn

      std::string contextStr = std::string("LocalNodeConn (acquire stream)");
      LogContext log(contextStr);

      establishedConns++;

      log.log(Log_DEBUG, "Establishing new stream connection to: internal");

      try
      {
         worker = new LocalConnWorker(StringTk::intToStr(++numCreatedWorkers) );

         worker->start();

         log.log(Log_DEBUG, "Connected: internal");
      }
      catch(ComponentInitException& e)
      {
         log.log(Log_WARNING, "Internal connect failed. Exception: " + std::string(e.what() ) );
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
   }

   if(!worker)
      throw SocketConnectException("Local connection attempt failed.");

   return worker->getClientEndpoint();
}

void LocalNodeConnPool::releaseStreamSocket(Socket* sock)
{
   const std::lock_guard<Mutex> lock(mutex);

   UnixConnWorkerListIter iter = connWorkerList.begin();

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

      UnixConnWorker* worker = *iter;
      worker->setAvailable(true);

      changeCond.signal();
   }
}

void LocalNodeConnPool::invalidateStreamSocket(Socket* sock)
{
   std::string contextStr = std::string("LocalNodeConn (invalidate stream: ") +
      sock->getPeername() + ")";
   LogContext log(contextStr);

   bool sockValid = true;
   UnixConnWorker* worker = NULL;

   {
      const std::lock_guard<Mutex> lock(mutex);

      UnixConnWorkerListIter iter = connWorkerList.begin();

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
   }

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

bool LocalNodeConnPool::updateInterfaces(unsigned short streamPort, const NicAddressList& nicList)
{
   const std::lock_guard<Mutex> lock(nicListMutex);
   // ignore streamPort
   bool changed = false;
   if (this->nicList.size() != nicList.size() ||
      !std::equal(this->nicList.begin(), this->nicList.end(), nicList.begin()))
   {
      changed = true;
      this->nicList = nicList;
   }
   return changed;
}
