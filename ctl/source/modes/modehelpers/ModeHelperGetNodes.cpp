#include <boost/optional.hpp>
#include <chrono>

#include <app/App.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/storage/StatStoragePathMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/toolkit/MathTk.h>
#include <common/toolkit/TimeTk.h>
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
namespace {
   template<typename Durations>
   void printPingAggregates(std::ostream &os, Durations&& times)
   {
         auto const total = std::accumulate (times.begin(), times.end(),
               typename Durations::value_type{});

         auto const average = total / times.size();

         os << "Ping (w/o 1st value) average: " << TimeTk::print_nice_time (average);

         // Median requires sorted vector.
         std::sort(times.begin(), times.end());

         if(auto const median = MathTk::medianOfSorted(times)) {
            os << ", median: " << TimeTk::print_nice_time(*median);
         }

         os << " (" << times.size() << " successful pings)" << std::endl;
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

   using Clock = TimeTk::highest_resolution_steady_clock;

   unsigned const numPingsPerNode = numPingRetries+1; // first ping to establish connection

   std::vector<TimeTk::as_double_duration<Clock::duration>> pingTimes;
   pingTimes.reserve(numPingRetries * nodes.size()); // Without first pings.

   std::cout << "Pinging... (" << nodes.size() << " nodes)" << std::endl;

   // loop: walk all nodes in list
   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
   {
      Node& node = **iter;

      // loop: ping this node
      for(unsigned numPingsDone = 0; numPingsDone < numPingsPerNode; numPingsDone++)
      {
         HeartbeatRequestMsg msg;

         auto const start = Clock::now();

         {
            const auto respMsg = MessagingTk::requestResponse(node, msg, NETMSGTYPE_Heartbeat);

            if (!respMsg)
            {
               std::cerr << "Node " << node.getID() << ": " << "Communication error." << std::endl;
               break;
            }
         }

         auto const duration = Clock::now() - start;

         std::cout << "Node " << node.getID() << ": " << TimeTk::print_nice_time (duration)
               << std::endl;

         if(numPingsDone)
         {  // First ping is ignored because of connection establishment overhead.
            pingTimes.emplace_back(duration);
         }
      }
   }

   if (pingTimes.size() > 1)
      printPingAggregates(std::cout, std::move(pingTimes));

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

      connPool->setLocalNicList(localNicList, tmpRdmaNicCaps);

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

   connPool->setLocalNicList(localNicList, tmpTcpNicCaps);

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
   connPool->setLocalNicList(localNicList, origNicCaps);

   std::cout << std::endl;
}

