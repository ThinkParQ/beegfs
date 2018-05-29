#include <app/App.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/toolkit/VersionTk.h>
#include <program/Program.h>
#include <modes/modehelpers/ModeHelperGetNodes.h>
#include "ModeGetNodes.h"


#define NODEINFO_INDENTATION_STR   "   "
#define NIC_INDENTATION_STR        "+ "

#define MODEGETNODES_ARG_PRINTDETAILS              "--details"
#define MODEGETNODES_ARG_PRINTNICDETAILS           "--nicdetails"
#define MODEGETNODES_ARG_PRINTFHGFSVERSION         "--showversion"
#define MODEGETNODES_ARG_CHECKREACHABILITY         "--reachable"
#define MODEGETNODES_ARG_ERRORCODES                "--errorcodes"
#define MODEGETNODES_ARG_REACHABILITYRETRIES       "--reachretries"
#define MODEGETNODES_ARG_REACHABILITYTIMEOUT_MS    "--reachtimeout"
#define MODEGETNODES_ARG_PING                      "--ping"
#define MODEGETNODES_ARG_PINGRETRIES               "--pingretries"
#define MODEGETNODES_ARG_CONNTESTNUM               "--numconns"
#define MODEGETNODES_ARG_CONNROUTE                 "--route"


int ModeGetNodes::execute()
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();

   const int mgmtTimeoutMS = 2500;

   std::vector<std::shared_ptr<Node>> nodes;
   std::set<NumNodeID> unreachableNodes;

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   // check arguments
   StringMapIter iter = cfg->find(MODEGETNODES_ARG_PRINTDETAILS);
   if(iter != cfg->end() )
   {
      cfgPrintDetails = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_PRINTNICDETAILS);
   if(iter != cfg->end() )
   {
      cfgPrintNicDetails = true;
      cfgPrintDetails = true; // implied in this case
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_PRINTFHGFSVERSION);
   if(iter != cfg->end() )
   {
      cfgPrintFhgfsVersion = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_CHECKREACHABILITY);
   if(iter != cfg->end() )
   {
      cfgCheckReachability = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_ERRORCODES);
   if(iter != cfg->end() )
   {
      cfgNotReachableAsError = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_REACHABILITYRETRIES);
   if(iter != cfg->end() )
   {
      cfgReachabilityNumRetries = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_REACHABILITYTIMEOUT_MS);
   if(iter != cfg->end() )
   {
      cfgReachabilityRetryTimeoutMS = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_PING);
   if(iter != cfg->end() )
   {
      cfgPing = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_PINGRETRIES);
   if(iter != cfg->end() )
   {
      cfgPingRetries = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_CONNTESTNUM);
   if(iter != cfg->end() )
   {
      cfgConnTestNum = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETNODES_ARG_CONNROUTE);
   if(iter != cfg->end() )
   {
      cfgPrintConnRoute = true;
      cfg->erase(iter);
   }


   NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg);
   NumNodeID rootNodeID; // Note: Only needed for NODETYPE_Meta.

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   if (nodeType != NODETYPE_Client && nodeType != NODETYPE_Meta && nodeType != NODETYPE_Storage
         && nodeType != NODETYPE_Mgmt)
   {
      std::cerr << "Invalid or missing node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   // Download the nodes into a list, not into the nodeStore yet*
   // The ones that actually reply to our heartbeat request will be sorted into the node store by
   // checkReachability.
   if (!NodesTk::waitForMgmtHeartbeat(
            NULL, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP, mgmtTimeoutMS))
   {
      std::cerr << "Management node communication failed: " << mgmtHost << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   if (!NodesTk::downloadNodes(*mgmtNode, nodeType, nodes, false, &rootNodeID))
   {
      std::cerr << "Node download failed." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   NodesTk::applyLocalNicCapsToList(app->getLocalNode(), nodes);

   // check reachability
   if(cfgCheckReachability)
      ModeHelperGetNodes::checkReachability(nodeType, nodes, unreachableNodes,
         cfgReachabilityNumRetries, cfgReachabilityRetryTimeoutMS);

   // ping
   if(cfgPing)
      ModeHelperGetNodes::pingNodes(nodeType, nodes, cfgPingRetries);

   // conn test
   if(cfgConnTestNum)
      ModeHelperGetNodes::connTest(nodeType, nodes, cfgConnTestNum);

   // print nodes
   printNodes(nodes, unreachableNodes, rootNodeID);

   if(cfgNotReachableAsError && !unreachableNodes.empty() )
      return APPCODE_NODE_NOT_REACHABLE;

   return APPCODE_NO_ERROR;
}

void ModeGetNodes::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --nodetype=<nodetype>  The node type (management, metadata," << std::endl;
   std::cout << "                         storage, client)." << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --details              Print additional node details, such as network ports" << std::endl;
   std::cout << "                         and interface order." << std::endl;
   std::cout << "  --nicdetails           Print additional network interconnect details, such as" << std::endl;
   std::cout << "                         IP address of each node interface." << std::endl;
   std::cout << "  --showversion          Print node version code." << std::endl;
   std::cout << "  --reachable            Check node reachability (from localhost)." << std::endl;
   std::cout << "  --errorcodes           Exit code reports an error if a node is not reachable," << std::endl;
   std::cout << "                         requires the option --reachable." << std::endl;
   std::cout << "  --reachretries=<num>   Number of retries for reachability check." << std::endl;
   std::cout << "                         (Default: 6)" << std::endl;
   std::cout << "  --reachtimeout=<ms>    Timeout in ms for reachability check retry." << std::endl;
   std::cout << "                         (Default: 500)" << std::endl;
   std::cout << "  --route                Print IP and protocol type through which the localhost" << std::endl;
   std::cout << "                         can connect to a node." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode queries the management daemon for information about registered" << std::endl;
   std::cout << " clients and servers." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: List registered storage nodes with their network interface order" << std::endl;
   std::cout << "  $ beegfs-ctl --listnodes --nodetype=storage --details" << std::endl;
}

void ModeGetNodes::printNodes(const std::vector<NodeHandle>& nodes,
   const std::set<NumNodeID>& unreachableNodes, NumNodeID rootNodeID)
{
   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
   {
      Node& node = **iter;

      // print only string ID for clients, but numeric & string ID for servers
      std::cout << node.getTypedNodeID() << std::endl;

      if(cfgPrintFhgfsVersion)
         printFhgfsVersion(node);

      if(cfgPrintDetails)
      {
         printPorts(node);
         printNicList(node);
      }

      if(cfgCheckReachability)
         printReachability(node, unreachableNodes);

      if(cfgPrintConnRoute)
         printConnRoute(node, unreachableNodes);
   }

   if(cfgPrintDetails)
   {
      std::cout << std::endl;

      std::cout << "Number of nodes: " << nodes.size() << std::endl;

      if(rootNodeID != 0)
         std::cout << "Root: " << rootNodeID << std::endl;
   }

}

void ModeGetNodes::printNicList(Node& node)
{
   // list usable network interfaces
   NicAddressList nicList(node.getNicList() );

   std::string nicListStr;
   std::string extendedNicListStr;
   int i=0;
   for(NicAddressListIter nicIter = nicList.begin(); nicIter != nicList.end(); nicIter++, i++)
   {
      std::string nicTypeStr;

      if(nicIter->nicType == NICADDRTYPE_RDMA)
         nicTypeStr = "RDMA";
      else
      if(nicIter->nicType == NICADDRTYPE_SDP)
         nicTypeStr = "SDP";
      else
      if(nicIter->nicType == NICADDRTYPE_STANDARD)
         nicTypeStr = "TCP";
      else
         nicTypeStr = "Unknown";

      nicListStr += std::string(nicIter->name) + "(" + nicTypeStr + ")" + " ";


      if(i > 0)
         extendedNicListStr += "\n";

      extendedNicListStr += std::string(NODEINFO_INDENTATION_STR) + NIC_INDENTATION_STR;
      extendedNicListStr += NetworkInterfaceCard::nicAddrToStringLight(&(*nicIter) );
   }

   std::cout << NODEINFO_INDENTATION_STR << "Interfaces: ";

   // normal NIC info will be printed inline, extended details will be printed in separate lines

   if(!cfgPrintNicDetails)
      std::cout << nicListStr << std::endl;
   else
      std::cout << std::endl << extendedNicListStr << std::endl;
}

void ModeGetNodes::printFhgfsVersion(Node& node)
{
   unsigned fhgfsVersion = node.getFhgfsVersion();

   std::cout << NODEINFO_INDENTATION_STR << "Version code: " <<
      VersionTk::versionCodeToPseudoVersionStr(fhgfsVersion) << std::endl;
}

void ModeGetNodes::printPorts(Node& node)
{
   unsigned portUDP = node.getPortUDP();
   unsigned portTCP = node.getPortTCP();

   std::cout << NODEINFO_INDENTATION_STR << "Ports: " <<
      "UDP: " << portUDP << "; " <<
      "TCP: " << portTCP <<
      std::endl;
}

void ModeGetNodes::printGotRoot(Node& node, uint16_t rootNodeID)
{
   if(node.getNumID() == NumNodeID(rootNodeID) )
      std::cout << NODEINFO_INDENTATION_STR << "Root: <yes>" << std::endl;
}

void ModeGetNodes::printReachability(Node& node, const std::set<NumNodeID>& unreachableNodes)
{
   bool isReachable = unreachableNodes.count(node.getNumID()) == 0;

   std::cout << NODEINFO_INDENTATION_STR << "Reachable: " <<  (isReachable ? "<yes>" : "<no>") <<
      std::endl;
}

/**
 * Connect to a node and print the route (IP etc) that was chosen by the ConnPool.
 * This is intended to see e.g. whether a failover route was chosen.
 *
 * @param unreachableNodes will be used to skip conn attempts to contained nodes
 */
void ModeGetNodes::printConnRoute(Node& node, const std::set<NumNodeID>& unreachableNodes)
{
   std::cout << NODEINFO_INDENTATION_STR << "Route: ";

   // don't connect to clients

   if(node.getNodeType() == NODETYPE_Client)
   {
      std::cout << "<skipped client>" << std::endl;
      return;
   }

   // was node already determined to be unreachable by previous check?

   bool isReachable = unreachableNodes.count(node.getNumID()) == 0;

   if(!isReachable)
   {
      std::cout << "<skipped unreachable>" << std::endl;
      return;
   }

   // actual connection attempt

   NodeConnPool* connPool = node.getConnPool();

   try
   {
      Socket* sock = connPool->acquireStreamSocket();

      // connect succeeded

      std::string peerStr = sock->getPeername();
      std::string protocolStr = NetworkInterfaceCard::nicTypeToString(sock->getSockType() );

      std::cout << peerStr << " (protocol: " << protocolStr << ")" << std::endl;
   }
   catch(SocketException& e)
   {
      // connect failed

      std::cout << "<connect failed on all available routes>" << std::endl;
   }
}
