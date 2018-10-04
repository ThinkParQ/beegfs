#include <program/Program.h>
#include "NodeListRequestor.h"

NodeListRequestor::NodeListRequestor() : PThread("NodeListReq"),
   log("NodeListReq")
{
   App *app = Program::getApp();

   this->cfg = app->getConfig();
   this->dgramLis = app->getDatagramListener();
   this->metaNodes = app->getMetaNodes();
   this->storageNodes = app->getStorageNodes();
   this->mgmtNodes = app->getMgmtNodes();
   this->clientNodes = app->getClientNodes();
   this->metaBuddyGroups = app->getMetaBuddyGroupMapper();
   this->storageBuddyGroups = app->getStorageBuddyGroupMapper();
   this->workQueue = app->getWorkQueue();
}

void NodeListRequestor::run()
{
   try
   {
      log.log(Log_DEBUG, "Component started.");
      registerSignalHandler();

      requestLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch (std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }
}

void NodeListRequestor::requestLoop()
{
   unsigned intervalMS = REQUEST_LOOP_INTERVAL_MS;

   NodeHandle mgmtNode;

   while(!getSelfTerminate() )
   {
      if(mgmtNodes->getSize() == 0)
      {
         // we got no management node yet, so try to discover one and get some info about it
         bool getMgmtRes = getMgmtNodeInfo();
         if(!getMgmtRes)
         {
            log.log(Log_DEBUG, "Did not receive a heartbeat from management node!");

            goto wait_for_continue;
         }
      }

      // try to reference first mgmt node (which is at the moment the only one we can have until we
      // implement redundant mgmt nodes)
      mgmtNode = mgmtNodes->referenceFirstNode();
      if(mgmtNode)
      {
         log.log(Log_SPAM, "Request node lists.");

         // add a work packet for each type of node
         workQueue->addIndirectWork(new GetMetaNodesWork(mgmtNode, metaNodes, metaBuddyGroups) );
         workQueue->addIndirectWork(new GetStorageNodesWork(mgmtNode, storageNodes,
            storageBuddyGroups) );
         workQueue->addIndirectWork(new GetClientNodesWork(mgmtNode, clientNodes) );
      }
      else
      {
         log.log(Log_SPAM, "Unable to reference management node during requesting node list.");
      }

   wait_for_continue:
      // do nothing but wait for the time of intervalMS
      if (PThread::waitForSelfTerminateOrder(intervalMS))
         break;
   }
}

/*
 * Get the management node using the NodesTk
 */
bool NodeListRequestor::getMgmtNodeInfo()
{
   unsigned numRetries = MGMT_HEARTBEAT_NUM_RETRIES;
   unsigned tryTimeoutMS = MGMT_HEARTBEAT_TIMEOUT_MS;
   unsigned nameResolutionRetries = MGMT_NAME_RESOLUTION_RETRIES;

   for (unsigned i = 0; i <= numRetries; i++)
   {
      // broadcast heartbeats requests
      log.log(Log_SPAM, "Waiting for management node...");

      // get mgmtd node using NodesTk
      bool waitRes = NodesTk::waitForMgmtHeartbeat(this, dgramLis, mgmtNodes,
            cfg->getSysMgmtdHostLocked(), cfg->getConnMgmtdPortUDPLocked(), tryTimeoutMS,
            nameResolutionRetries);

      if(waitRes)
      {
         // if mgmtd node was found, immediately add a GetNodeInfoWork to get some general info on
         // that node

         auto node = mgmtNodes->referenceFirstNode();
         if(node)
         {
            Work* work = new GetNodeInfoWork(node->getNumID(), NODETYPE_Mgmt);
            Program::getApp()->getWorkQueue()->addIndirectWork(work);
         }

         return true;
      }

      /* if the first time did not succeed, wait for tryTimeoutMS and try again until numRetries is
         reached */

      if(PThread::waitForSelfTerminateOrder(tryTimeoutMS) )
         break;
   }

   return false;
}

