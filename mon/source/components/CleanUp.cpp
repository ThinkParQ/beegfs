#include "CleanUp.h"

#include <app/App.h>

CleanUp::CleanUp(App* app) :
   PThread("CleanUp"), app(app)
{}

void CleanUp::run()
{
   try
   {
      LOG(GENERAL, DEBUG, "Component started.");
      registerSignalHandler();
      loop();
      LOG(GENERAL, DEBUG, "Component stopped.");
   }
   catch (std::exception& e)
   {
      app->handleComponentException(e);
   }
}

void CleanUp::loop()
{
   const std::chrono::minutes idleDisconnectInterval(30);

   while (!waitForSelfTerminateOrder(std::chrono::milliseconds(idleDisconnectInterval).count()))
   {
      dropIdleConns();
   }
}

void CleanUp::dropIdleConns()
{
   unsigned numDroppedConns = 0;

   numDroppedConns += dropIdleConnsByStore(app->getMgmtNodes());
   numDroppedConns += dropIdleConnsByStore(app->getMetaNodes());
   numDroppedConns += dropIdleConnsByStore(app->getStorageNodes());

   if (numDroppedConns)
   {
      LOG(GENERAL, DEBUG, "Idle connections dropped", numDroppedConns);
   }
}

unsigned CleanUp::dropIdleConnsByStore(NodeStoreServers* nodes)
{
   unsigned numDroppedConns = 0;

   const auto referencedNodes = nodes->referenceAllNodes();
   for (auto node = referencedNodes.begin(); node != referencedNodes.end();
         node++)
   {
      // don't do any idle disconnect stuff with local node
      // (the LocalNodeConnPool doesn't support and doesn't need this kind of treatment)
      if (*node != app->getLocalNode())
      {
         auto connPool = (*node)->getConnPool();

         numDroppedConns += connPool->disconnectAndResetIdleStreams();
      }
   }

   return numDroppedConns;
}
