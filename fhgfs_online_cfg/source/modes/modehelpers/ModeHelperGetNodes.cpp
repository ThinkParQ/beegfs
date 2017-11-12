#include <app/App.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/TimeFine.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/storage/StatStoragePathMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <program/Program.h>
#include "ModeHelper.h"
#include "ModeHelperGetNodes.h"


/**
 * @param numRetries must be >=1
 */
void ModeHelperGetNodes::checkReachability(NodeType nodeType, const std::vector<NodeHandle>& nodes,
   std::set<NumNodeID>& outUnreachableNodes, unsigned numRetries, unsigned retryTimeoutMS)
{
   HeartbeatRequestMsg msg;

   App* app = Program::getApp();
   NodeStoreServers* serverStore = app->getServerStoreFromType(nodeType);
   NodeStoreClients* clientStore = app->getClientNodes();
   DatagramListener* dgramLis = app->getDatagramListener();

   outUnreachableNodes.clear();
   for (auto it = nodes.begin(); it != nodes.end(); it++)
      outUnreachableNodes.insert((*it)->getNumID());

   for (; numRetries > 0 && !outUnreachableNodes.empty(); numRetries--)
   {
      // request heartbeat from all nodes
      dgramLis->sendToNodesUDP(nodes, &msg, 0);

      // wait for responses
      PThread::sleepMS(retryTimeoutMS);

      // remove responding nodes from unreachable list
      for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
      {
         auto& currentNode = **iter;
         NodeHandle referencedNode;

         if (nodeType == NODETYPE_Client)
            referencedNode = clientStore->referenceNode(currentNode.getNumID());
         else
            referencedNode = serverStore->referenceNode(currentNode.getNumID());

         if (referencedNode)
            outUnreachableNodes.erase(currentNode.getNumID());
      }
   }
}

/**
 * Ping nodes via stream connection and print results.
 *
 * @param numPingRetries 0-based number of ping repetitions per node
 */
void ModeHelperGetNodes::pingNodes(NodeType nodeType, const std::vector<NodeHandle>& nodes,
   unsigned numPingRetries)
{
   // note: this works by sending heartbeat requests to all nodes via stream connection and
   //    measuring response time.

   unsigned numPingsPerNode = numPingRetries+1;
   size_t numPingRequests = 0; // ignores initial result (warmup)
   size_t pingTotalMicroSecs = 0; // micro seconds
   size_t pingTotalMS = 0;

   std::cout << "Pinging... (" << nodes.size() << " nodes)" << std::endl;

   // loop: walk all nodes in list
   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
   {
      Node& node = **iter;

      // loop: ping this node
      for(unsigned numPingsDone = 0; numPingsDone < numPingsPerNode; numPingsDone++)
      {
         HeartbeatRequestMsg msg;
         char* respBuf = NULL;
         NetMessage* respMsg = NULL;

         TimeFine pingStartTime;

         // request/response
         bool commRes = MessagingTk::requestResponse(
            node, &msg, NETMSGTYPE_Heartbeat, &respBuf, &respMsg);
         if(unlikely(!commRes) )
         {
            std::cerr << "Node " << node.getID() << ": " << "Communication error." << std::endl;
            break;
         }

         TimeFine pingEndTime;

         unsigned pingTimeMS = pingEndTime.elapsedSinceMS(&pingStartTime);
         unsigned pingTimeMicroSecs = pingEndTime.elapsedSinceMicro(&pingStartTime);

         if(pingTimeMS > 9)
            std::cout << "Node " << node.getID() << ": " << pingTimeMS << "ms" << std::endl;
         else
            std::cout << "Node " << node.getID() << ": " << pingTimeMicroSecs << "us" << std::endl;


         if(numPingsDone)
         { // first ping is ignored because of connection establishment overhead
            pingTotalMS += pingTimeMS;
            pingTotalMicroSecs += pingTimeMicroSecs;
            numPingRequests++;
         }

         delete(respMsg);
         free(respBuf);
      }
   }

   // print average

   if(numPingRequests > 1)
   { // note: we print µs only if avg ping ms < 10
      unsigned pingAvgIntMS = (pingTotalMS / numPingRequests);

      if(pingAvgIntMS >= 10)
      { // don't print µs
         printf("Ping average (w/o 1st value): %ums (%u successful pings)\n",
            pingAvgIntMS, (unsigned)numPingRequests);
      }
      else
      { // print µs
         float pingAvgFloatMS = (pingTotalMicroSecs / numPingRequests) / 1000.0;

         printf("Ping average (w/o 1st value): %0.3fms (%u successful pings)\n",
            pingAvgFloatMS, (unsigned)numPingRequests);
      }
   }

   std::cout << std::endl;
}


/**
 * Establish the given number of connections simultaneously to each of the nodes.
 * This is intended to test general limit and performance impacts with a high number of established
 * connections.
 *
 * @param numPingRetries 0-based number of ping repetitions per node
 */
void ModeHelperGetNodes::connTest(NodeType nodeType, const std::vector<NodeHandle>& nodes,
   unsigned numConns)
{
   // note: this works by sending heartbeat requests to all nodes via stream connection and
   //    measuring response time.

   if (nodes.empty())
   {
      std::cerr << "Skipping conn test (no nodes available)." << std::endl;
      return;
   }

   App* app = Program::getApp();
   Node& localNode = app->getLocalNode();
   Node& node = **nodes.begin();
   NodeConnPool* connPool = node.getConnPool();
   NicAddressList origNicList = connPool->getNicList();
   NicListCapabilities origNicCaps;
   unsigned i = 0;

   Socket** socks = (Socket**)calloc(1, numConns * sizeof(Socket*) );

   NetworkInterfaceCard::supportedCapabilities(&origNicList, &origNicCaps);

   connPool->setMaxConns(numConns+1); // temporarily inc conn limit of node

   NicAddressList localNicList(localNode.getNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);

   if(localNicCaps.supportsRDMA)
   {
      std::cout << "Beginning RDMA conn test. Node: " << node.getID() << std::endl;

      NicListCapabilities tmpRdmaNicCaps = localNicCaps;
      tmpRdmaNicCaps.supportsSDP = false;

      connPool->setLocalNicCaps(&tmpRdmaNicCaps);

      for(i=0; i < numConns; i++)
      {
         // log status message for each 500 established conns
         if( (i % 500) == 0)
            std::cout << "Established connections: " << i << std::endl;

         try
         {
            // establish new connection
            socks[i] = connPool->acquireStreamSocket();
         }
         catch(SocketException& e)
         {
            std::cerr << "Connect error: " << e.what() << std::endl;
         }

         if(!socks[i])
         {
            std::cerr << "RDMA Connect #" << (i+1) << " failed." << std::endl;
            break;
         }

         if(socks[i]->getSockType() != NICADDRTYPE_RDMA)
         {
            std::cerr << "Connection #" << (i+1) << " is not of type RDMA." << std::endl;
            break;
         }

      }

      std::cout << "Hit ENTER to drop " << i << " RDMA connections... "; // (no endl here)
      getchar();

      for(i=0; i < numConns; i++)
      {
         if(socks[i])
            connPool->invalidateStreamSocket(socks[i]);
      }
   } // end of RDMA


   // TCP test...

   std::cout << "Beginning TCP conn test. Node: " << node.getID() << std::endl;

   NicListCapabilities tmpTcpNicCaps = localNicCaps;
   tmpTcpNicCaps.supportsRDMA = false;
   tmpTcpNicCaps.supportsSDP = false;

   connPool->setLocalNicCaps(&tmpTcpNicCaps);

   memset(socks, 0, numConns * sizeof(Socket*) );

   for(i=0; i < numConns; i++)
   {
      // log status message for each 500 established conns
      if( (i % 500) == 0)
         std::cout << "Established connections: " << i << std::endl;

      try
      {
         // establish new connection
         socks[i] = connPool->acquireStreamSocket();
      }
      catch(SocketException& e)
      {
         std::cerr << "Connect error: " << e.what() << std::endl;
      }

      if(!socks[i])
      {
         std::cerr << "TCP Connect #" << (i+1) << " failed." << std::endl;
         break;
      }

      if(socks[i]->getSockType() != NICADDRTYPE_STANDARD)
      {
         std::cerr << "Connection #" << (i+1) << " is not of type STANDARD." << std::endl;
         break;
      }
   }

   std::cout << "Hit ENTER to drop " << i << " TCP connections... "; // (no std::endl here)
   getchar();

   for(i=0; i < numConns; i++)
   {
      if(socks[i])
         connPool->invalidateStreamSocket(socks[i]);
   }

   // cleanup
   free(socks);
   connPool->setMaxConns(app->getConfig()->getConnMaxInternodeNum() );
   connPool->setLocalNicCaps(&origNicCaps);

   std::cout << std::endl;
}

