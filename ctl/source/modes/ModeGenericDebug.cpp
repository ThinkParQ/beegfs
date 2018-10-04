#include <app/App.h>
#include <common/net/message/nodes/GenericDebugMsg.h>
#include <common/net/message/nodes/GenericDebugRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/TimeAbs.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeGenericDebug.h"


#define MODEGENERICDEBUG_ARG_NODEID     "--nodeid"


int ModeGenericDebug::execute()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   std::string nodeIDStr;
   NodeStoreServers* nodeStore;
   std::string commandStr;
   std::string cmdRespStr;
   bool commRes;

   StringMapIter iter;

   // check privileges

   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments

   NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg);
   if(nodeType == NODETYPE_Invalid)
   {
      std::cerr << "Invalid or missing node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else
   if( (nodeType != NODETYPE_Storage) && (nodeType != NODETYPE_Meta) &&
       (nodeType != NODETYPE_Mgmt) )
   {
      std::cerr << "Invalid node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   iter = cfg->find(MODEGENERICDEBUG_ARG_NODEID);
   if(iter == cfg->end() )
   { // nodeID not specified
      std::cerr << "NodeID not specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else
   {
      nodeIDStr = iter->second;
      cfg->erase(iter);
   }

   if(cfg->empty() )
   { // command string not specified
      std::cerr << "Command string not specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else
   {
      commandStr = cfg->begin()->first;
      cfg->erase(cfg->begin() );
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   nodeStore = app->getServerStoreFromType(nodeType);

   // send command and recv response

   commRes = communicate(nodeStore, nodeIDStr, commandStr, &cmdRespStr);
   if(!commRes)
      return APPCODE_RUNTIME_ERROR;

   // communication succeeded => print response

   std::cout << "Command: " << commandStr << std::endl;
   std::cout << "Response:" << std::endl;
   std::cout << cmdRespStr << std::endl;

   return APPCODE_NO_ERROR;
}

void ModeGenericDebug::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --nodeid=<nodeID>      Send command to node with this ID." << std::endl;
   std::cout << "  --nodetype=<nodetype>  The node type of the recipient (metadata, storage)." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode sends generic debug commands to a certain node." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Execute debug command" << std::endl;
   std::cout << "  # beegfs-ctl --genericdebug <debug_command>" << std::endl;
}

/**
 * @param nodeID can be numNodeID (fast) or stringNodeID (slow).
 * @param outCmdRespStr response from the node
 * @return false if communication error occurred
 */
bool ModeGenericDebug::communicate(NodeStoreServers* nodes, std::string nodeID, std::string cmdStr,
   std::string* outCmdRespStr)
{
   bool retVal = false;
   NodeHandle node;

   if(StringTk::isNumeric(nodeID) )
   {
      NumNodeID nodeNumID(StringTk::strToUInt(nodeID) );
      node = nodes->referenceNode(nodeNumID);
   }
   else
   {
      auto allNodes = nodes->referenceAllNodes();
      auto nodeIt = std::find_if(allNodes.begin(), allNodes.end(),
            [&] (const auto& node) { return node->getID() == nodeID; });
      node = nodeIt != allNodes.end()
         ? *nodeIt
         : nullptr;
   }

   if(!node)
   {
      std::cerr << "Unknown node: " << nodeID << std::endl;
      return false;
   }

   GenericDebugRespMsg* respMsgCast;

   GenericDebugMsg msg(cmdStr.c_str() );

   const auto respMsg = MessagingTk::requestResponse(*node, msg, NETMSGTYPE_GenericDebugResp);
   if (!respMsg)
   {
      std::cerr << "Failed to communicate with node: " << nodeID << std::endl;
      goto err_cleanup;
   }

   respMsgCast = (GenericDebugRespMsg*)respMsg.get();

   *outCmdRespStr = respMsgCast->getCmdRespStr();

   retVal = true;

err_cleanup:
   return retVal;
}

