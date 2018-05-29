#include "NodeListRequestor.h"

#include <common/toolkit/NodesTk.h>
#include <components/worker/GetNodesWork.h>

#include <app/App.h>

static const unsigned MGMT_NUM_TRIES = 3;
static const std::chrono::milliseconds MGMT_TIMEOUT{1000};

NodeListRequestor::NodeListRequestor(App* app) :
   PThread("NodeListReq"), app(app)
{}

void NodeListRequestor::run()
{
   try
   {
      LOG(GENERAL, DEBUG, "Component started.");
      registerSignalHandler();

      requestLoop();

      LOG(GENERAL, DEBUG, "Component stopped.");
   }
   catch (std::exception& e)
   {
      app->handleComponentException(e);
   }
}

void NodeListRequestor::requestLoop()
{
   do
   {
      // Get management node. Do this every time before updating node lists to check if
      // management is online to prevent log spam from NodesTk::downloadNodes when it is
      // not reachable
      if (!getMgmtNodeInfo())
      {
         LOG(GENERAL, NOTICE, "Did not receive a response from management node!");
         continue;
      }

      // try to reference first mgmt node (which is at the moment the only one)
      std::shared_ptr<Node> mgmtNode = app->getMgmtNodes()->referenceFirstNode();

      if (mgmtNode)
      {
         LOG(GENERAL, DEBUG, "Requesting node lists...");

         app->getWorkQueue()->addIndirectWork(new GetNodesWork(mgmtNode, app->getMetaNodes(),
               NODETYPE_Meta, app->getMetaBuddyGroupMapper(), app->getLocalNode()));
         app->getWorkQueue()->addIndirectWork(new GetNodesWork(mgmtNode,
               app->getStorageNodes(), NODETYPE_Storage, app->getStorageBuddyGroupMapper(),
               app->getLocalNode()));
      }
      else
      {
         LOG(GENERAL, DEBUG, "Unable to reference management node for node list request.");
      }
   }
   while (!waitForSelfTerminateOrder(std::chrono::milliseconds(
         app->getConfig()->getNodelistRequestInterval()).count()));
}

bool NodeListRequestor::getMgmtNodeInfo()
{
   for (unsigned i = 0; i < MGMT_NUM_TRIES; i++)
   {
      LOG(GENERAL, DEBUG, "Waiting for management node...");

      // get mgmtd node using NodesTk
      auto mgmtNode = NodesTk::downloadNodeInfo(app->getConfig()->getSysMgmtdHost(),
         app->getConfig()->getConnMgmtdPortUDP(), app->getConfig()->getConnAuthHash(),
         app->getNetMessageFactory(),
         NODETYPE_Mgmt, MGMT_TIMEOUT.count());

      if(mgmtNode)
      {
         app->getMgmtNodes()->addOrUpdateNodeEx(std::move(mgmtNode), nullptr);
         return true;
      }

      if (PThread::waitForSelfTerminateOrder(std::chrono::milliseconds(MGMT_TIMEOUT).count()))
         break;
   }

   return false;
}

