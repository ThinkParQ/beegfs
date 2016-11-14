#include <common/toolkit/Time.h>
#include <program/Program.h>
#include <app/App.h>
#include "InternodeSyncer.h"


InternodeSyncer::InternodeSyncer() throw(ComponentInitException) : PThread("XNodeSync"),
   log("XNodeSync")
{
}

InternodeSyncer::~InternodeSyncer()
{
}


void InternodeSyncer::run()
{
   try
   {
      registerSignalHandler();

      syncLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

void InternodeSyncer::syncLoop()
{
   App* app = Program::getApp();

   const int sleepIntervalMS = 15*60*1000; // 15min

   const unsigned cleanupRequestorClientStatsMS = 30*60*1000; // 30min
   const unsigned idleDisconnectIntervalMS = 70*60*1000; /* 70 minutes (must be less than half the
      streamlis idle disconnect interval to avoid cases where streamlis disconnects first) */

   Time lastCleanupRequestorClientStatsT;
   Time lastIdleDisconnectT;

   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      if(lastCleanupRequestorClientStatsT.elapsedMS() > cleanupRequestorClientStatsMS)
      {
         app->getClientStatsOperator()->cleanupStoppedRequestors();
         lastCleanupRequestorClientStatsT.setToNow();
      }

      if(lastIdleDisconnectT.elapsedMS() > idleDisconnectIntervalMS)
      {
         dropIdleConns();
         lastIdleDisconnectT.setToNow();
      }
   }
}

/**
 * Drop/reset idle conns from all server stores.
 */
void InternodeSyncer::dropIdleConns()
{
   App* app = Program::getApp();

   unsigned numDroppedConns = 0;

   numDroppedConns += dropIdleConnsByStore(app->getMgmtNodes() );
   numDroppedConns += dropIdleConnsByStore(app->getMetaNodes() );
   numDroppedConns += dropIdleConnsByStore(app->getStorageNodes() );

   if(numDroppedConns)
   {
      log.log(Log_DEBUG, "Dropped idle connections: " + StringTk::uintToStr(numDroppedConns) );
   }
}

/**
 * Walk over all nodes in the given store and drop/reset idle connections.
 *
 * @return number of dropped connections
 */
unsigned InternodeSyncer::dropIdleConnsByStore(NodeStoreServers* nodes)
{
   App* app = Program::getApp();

   unsigned numDroppedConns = 0;

   auto node = nodes->referenceFirstNode();
   while(node)
   {
      /* don't do any idle disconnect stuff with local node
         (the LocalNodeConnPool doesn't support and doesn't need this kind of treatment) */

      if (node.get() != &app->getLocalNode())
      {
         NodeConnPool* connPool = node->getConnPool();

         numDroppedConns += connPool->disconnectAndResetIdleStreams();
      }

      node = nodes->referenceNextNode(node); // iterate to next node
   }

   return numDroppedConns;
}
