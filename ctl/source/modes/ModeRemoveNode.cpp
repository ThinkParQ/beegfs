#include <app/App.h>
#include <common/net/message/nodes/RemoveNodeMsg.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/toolkit/ZipIterator.h>
#include <modes/modehelpers/ModeHelperGetNodes.h>
#include <program/Program.h>
#include "ModeRemoveNode.h"


#define MODEREMOVENODE_ARG_REMOVEUNREACHABLE      "--unreachable"
#define MODEREMOVENODE_ARG_REACHABILITYRETRIES    "--reachretries"
#define MODEREMOVENODE_ARG_REACHABILITYTIMEOUT_MS "--reachtimeout"
#define MODEREMOVENODE_ARG_FORCE                  "--force"



int ModeRemoveNode::execute()
{
   int retVal = APPCODE_NO_ERROR;

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments

   StringMapIter iter = cfg->find(MODEREMOVENODE_ARG_REMOVEUNREACHABLE);
   if(iter != cfg->end() )
   {
      cfgRemoveUnreachable = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEREMOVENODE_ARG_REACHABILITYRETRIES);
   if(iter != cfg->end() )
   {
      cfgReachabilityNumRetries = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEREMOVENODE_ARG_REACHABILITYTIMEOUT_MS);
   if(iter != cfg->end() )
   {
      cfgReachabilityRetryTimeoutMS = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEREMOVENODE_ARG_FORCE);
   if(iter != cfg->end() )
   {
      cfgForce = true;
      cfg->erase(iter);
   }

   NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg);
   switch(nodeType)
   {
      case NODETYPE_Client:
      case NODETYPE_Meta:
      case NODETYPE_Storage:
         break;

      default:
      {
         std::cerr << "Invalid or missing node type." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }

   if(!cfgRemoveUnreachable && cfg->empty() )
   { // user forgot to give us a nodeID
      std::cerr << "No node ID specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else
   if(!cfg->empty() )
   {
      cfgNodeID = cfg->begin()->first;
      cfg->erase(cfg->begin() );
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   // get typed nodeID (i.e. short numeric ID for servers, long ID for clients)

   std::string typedNodeID;
   NumNodeID typedNodeNumID;

   if(!cfgRemoveUnreachable)
   {
      bool isNumericRes = StringTk::isNumeric(cfgNodeID);
      if(!isNumericRes)
      {
         std::cerr << "Invalid nodeID given (must be numeric): " << cfgNodeID << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      typedNodeNumID = NumNodeID(StringTk::strToUInt(cfgNodeID) );
   }

   // the actual remove workhorses
   if(cfgRemoveUnreachable)
      retVal = removeUnreachableNodes(nodeType);
   else
   {
      bool isPartOfMBG = false;

      if(!cfgForce)
      {
         auto mgmtNode = mgmtNodes->referenceFirstNode();
         if (!mgmtNode)
         {
            std::cerr << "Failed to reference management node." << std::endl;
            retVal = APPCODE_RUNTIME_ERROR;
         }

         if(isPartOfMirrorBuddyGroup(*mgmtNode, typedNodeNumID, nodeType, &isPartOfMBG) )
            retVal = APPCODE_RUNTIME_ERROR;
      }

      if(!retVal && (cfgForce || !isPartOfMBG) )
         retVal = removeSingleNode(typedNodeNumID, nodeType);
   }

   return retVal;
}

void ModeRemoveNode::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <nodeID>               Node ID that should be removed (e.g. one of the IDs" << std::endl;
   std::cout << "                         returned by mode \"--listnodes\"). Short numeric ID for" << std::endl;
   std::cout << "                         servers, long ID for clients." << std::endl;
   std::cout << "  --nodetype=<nodetype>  The node type (metadata, storage, client)." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --unreachable          Remove all nodes that are currently unreachable (from" << std::endl;
   std::cout << "                         the localhost). In this case, the <nodeID> argument is" << std::endl;
   std::cout << "                         ignored." << std::endl;
   std::cout << "  --reachretries=<num>   Number of retries for reachability check." << std::endl;
   std::cout << "                         (Default: 6)" << std::endl;
   std::cout << "  --reachtimeout=<ms>    Timeout in ms for reachability check per retry." << std::endl;
   std::cout << "                         (Default: 500)" << std::endl;
   std::cout << "  --force                Remove the node also when a target is member of a" << std::endl;
   std::cout << "                         MirrorBuddyGroup." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode removes a certain node from the file system by unregistering it at" << std::endl;
   std::cout << " the management server." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Remove storage server with ID \"5\"" << std::endl;
   std::cout << "  # beegfs-ctl --removenode --nodetype=storage 5" << std::endl;
}

/**
 * @return APPCODE_...
 */
int ModeRemoveNode::removeSingleNode(NumNodeID nodeNumID, NodeType nodeType)
{
   int retVal = APPCODE_RUNTIME_ERROR;

   FhgfsOpsErr removeRes = removeNodeComm(nodeNumID, nodeType);

   if(removeRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Failed to remove node: " << cfgNodeID <<
         " (Error: " << removeRes << ")" << std::endl;
   }
   else
   {
      std::cout << "Removal of node succeeded: " << cfgNodeID << std::endl;
      retVal = APPCODE_NO_ERROR;
   }

   return retVal;
}

/**
 * @return APPCODE_...
 */
int ModeRemoveNode::removeUnreachableNodes(NodeType nodeType)
{
   int retVal = APPCODE_NO_ERROR; // must initially be "no error" here (changed in removal loop)

   App* app = Program::getApp();

   AbstractNodeStore* nodeStore;
   if (nodeType == NODETYPE_Client)
      nodeStore = app->getClientNodes();
   else
      nodeStore = app->getServerStoreFromType(nodeType);

   std::set<NumNodeID> unreachableNodeIDs;

   unsigned numRemoved = 0;
   unsigned numRemoveErrors = 0; // removal errors

   // check reachability
   auto nodes = nodeStore->referenceAllNodes();
   ModeHelperGetNodes::checkReachability(nodeType, nodes, unreachableNodeIDs,
      cfgReachabilityNumRetries, cfgReachabilityRetryTimeoutMS);

   if(unreachableNodeIDs.empty() )
   {
      std::cout << "No unreachable nodes found." << std::endl;
      retVal = APPCODE_NO_ERROR;
      goto cleanup_nodes;
   }

   // walk over nodes list and identify those that are in the unreachable set
   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
   {
      Node& node = **iter;
      NumNodeID nodeNumID = node.getNumID();

      auto unreachableIter = unreachableNodeIDs.find(nodeNumID);
      if (unreachableIter == unreachableNodeIDs.end())
         continue; // node was reachable

      std::cout << "Removing node: " << node.getTypedNodeID() << std::endl;

      FhgfsOpsErr removeRes = removeNodeComm(nodeNumID, nodeType);

      if(removeRes == FhgfsOpsErr_SUCCESS)
         numRemoved++;
      else
      {
         std::cerr << "Removal of node failed: " << node.getTypedNodeID() <<
            " (Error: " << removeRes << ")" << std::endl;

         numRemoveErrors++;
         retVal = APPCODE_RUNTIME_ERROR;
      }
   }

   std::cout << std::endl;

   // print summary
   std::cout << "Number of removed nodes: " << numRemoved << " " <<
      "(Failed: " << numRemoveErrors << ")" << std::endl;

   // cleanup

cleanup_nodes:
   return retVal;
}


FhgfsOpsErr ModeRemoveNode::removeNodeComm(NumNodeID nodeNumID, NodeType nodeType)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   auto mgmtNode = mgmtNodes->referenceFirstNode();

   RemoveNodeRespMsg* respMsgCast;

   RemoveNodeMsg msg(nodeNumID, nodeType);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg, NETMSGTYPE_RemoveNodeResp);
   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << mgmtNode->getNodeIDWithTypeStr() <<
         std::endl;
      retVal = FhgfsOpsErr_COMMUNICATION;
      goto err_cleanup;
   }

   respMsgCast = (RemoveNodeRespMsg*)respMsg.get();

   retVal = (FhgfsOpsErr)respMsgCast->getValue();

err_cleanup:
   return retVal;
}

/**
 * Checks if the targets of a node are part of a MirrorBuddyGroup. This check accepts only nodeIDs
 * of storage servers.
 *
 * @param mgmtNode the management node, must be referenced before given to this method
 * @param nodeNumID the nodeID to check
 * @param nodeType the nodeType of the given nodeID
 * @param outIsPartOfMBG out value which is true if the target is part of a MBG, else false
 * @return 0 on success, all other values reports an error (FhgfsOpsErr_...)
 */
FhgfsOpsErr ModeRemoveNode::isPartOfMirrorBuddyGroup(Node& mgmtNode, NumNodeID nodeNumID,
   NodeType nodeType, bool* outIsPartOfMBG)
{
   *outIsPartOfMBG = false;

   if(nodeType == NODETYPE_Client) // check is only required for storage and metadata servers
      return FhgfsOpsErr_SUCCESS;


   App* app = Program::getApp();

   // download MirrorBuddyGroups

   UInt16List buddyGroupIDs;
   UInt16List buddyGroupPrimaryTargetIDs;
   UInt16List buddyGroupSecondaryTargetIDs;

   if(!NodesTk::downloadMirrorBuddyGroups(mgmtNode, nodeType, &buddyGroupIDs,
       &buddyGroupPrimaryTargetIDs, &buddyGroupSecondaryTargetIDs, false) )
   {
      std::cerr << "Download of mirror buddy groups failed." << std::endl;
      return FhgfsOpsErr_COMMUNICATION;
   }

   if (nodeType == NODETYPE_Meta)
   {
      for (ZipIterRange<UInt16List, UInt16List, UInt16List>
           iter(buddyGroupIDs, buddyGroupPrimaryTargetIDs, buddyGroupSecondaryTargetIDs);
           !iter.empty(); ++iter)
      {
         if(nodeNumID == NumNodeID(*iter()->second) || nodeNumID == NumNodeID(*iter()->third) )
         {
            std::cerr << "Unmapping node " << nodeNumID <<
               " damages the mirror buddy group " << *(iter()->first) << ". "
               "Restart the unmap command with the parameter --force to ignore this warning."
               << std::endl;

            *outIsPartOfMBG = true;
            break;
         }
      }
   }
   else
   {
      TargetMapper* targetMapper = app->getTargetMapper();

      // download targets

      auto mappings = NodesTk::downloadTargetMappings(mgmtNode, false);
      if (!mappings.first)
      {
         std::cerr << "Target mappings download failed." << std::endl;
         return FhgfsOpsErr_COMMUNICATION;
      }
      targetMapper->syncTargets(std::move(mappings.second));


      UInt16List targetList;
      targetMapper->getTargetsByNode(nodeNumID, targetList);
      for (UInt16ListIter targetIter = targetList.begin();
           targetIter != targetList.end(); targetIter++)
      {
         for (ZipIterRange<UInt16List, UInt16List, UInt16List>
              iter(buddyGroupIDs, buddyGroupPrimaryTargetIDs, buddyGroupSecondaryTargetIDs);
              !iter.empty(); ++iter )
         {
            if( (*(iter()->second) == *targetIter) || (*(iter()->third) == *targetIter) )
            {
               std::cerr << "Unmapping node " << nodeNumID << " unmaps the target " <<
                  *targetIter << " which damages the mirror buddy group "
                  << *(iter()->first) << ". "
                  "Restart the unmap command with the parameter --force to ignore this warning."
                  << std::endl;

               *outIsPartOfMBG = true;
               break;
            }
         }
      }
   }

   return FhgfsOpsErr_SUCCESS;
}
