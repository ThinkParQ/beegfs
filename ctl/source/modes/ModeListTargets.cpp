#include <app/App.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/net/message/storage/mirroring/GetStorageResyncStatsMsg.h>
#include <common/net/message/storage/mirroring/GetStorageResyncStatsRespMsg.h>
#include <common/nodes/TargetStateStore.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>
#include "ModeListTargets.h"

#include <algorithm>

#include <boost/lexical_cast.hpp>


#define MODELISTTARGETS_ARG_PRINTNODESFIRST     "--nodesfirst"
#define MODELISTTARGETS_ARG_PRINTLONGNODES      "--longnodes"
#define MODELISTTARGETS_ARG_HIDENODEID          "--hidenodeid"
#define MODELISTTARGETS_ARG_PRINTSTATE          "--state"
#define MODELISTTARGETS_ARG_PRINTCAPACITYPOOLS  "--pools"
#define MODELISTTARGETS_ARG_NODETYPE            "--nodetype"
#define MODELISTTARGETS_ARG_NODETYPE_META       "meta"
#define MODELISTTARGETS_ARG_NODETYPE_STORAGE    "storage"
#define MODELISTTARGETS_ARG_FROMMETA            "--frommeta" /* implies provided nodeID */
#define MODELISTTARGETS_ARG_PRINTSPACEINFO      "--spaceinfo"
#define MODELISTTARGETS_ARG_MIRRORGROUPS        "--mirrorgroups"
#define MODELISTTARGETS_ARG_ERRORCODES          "--errorcodes"
#define MODELISTTARGETS_ARG_PRINTSTORAGEPOOLS   "--storagepools"
#define MODELISTTARGETS_ARG_STORAGEPOOL         "--storagepoolid"

#define MODELISTTARGETS_MAX_LINE_LENGTH         256

#define MODELISTTARGETS_POOL_LOW                "low"
#define MODELISTTARGETS_POOL_NORMAL             "normal"
#define MODELISTTARGETS_POOL_EMERGENCY          "emergency"
#define MODELISTTARGETS_POOL_UNKNOWN            "unknown"

#define MODELISTTARGETS_BUDDYGROUP_MEMBERTYPE_PRIMARY       "primary"
#define MODELISTTARGETS_BUDDYGROUP_MEMBERTYPE_SECONDARY     "secondary"

#define MODELISTTARGETS_TARGET_CONSISTENCY_STATE_RESYNCING  "resyncing"



int ModeListTargets::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   // check arguments
   retVal = checkConfig();
   if(retVal)
      return retVal;

   if(prepareData() )
      return APPCODE_RUNTIME_ERROR;

   retVal = print();

   return retVal;
}

/**
 * checks all arguments of the mode and creates the configuration for the mode
 *
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::checkConfig()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   StringMapIter iter = cfg->find(MODELISTTARGETS_ARG_PRINTNODESFIRST);
   if(iter != cfg->end() )
   {
      cfgPrintNodesFirst = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_PRINTLONGNODES);
   if(iter != cfg->end() )
   {
      cfgPrintLongNodes = true;
      cfgPrintNodesFirst = false; // not allowed in combination with long node output format
      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_HIDENODEID);
   if(iter != cfg->end() )
   {
      cfgHideNodeID = true; // if --hidenodeid is set, the other both nodeID options are ignored
      cfgPrintLongNodes = false;
      cfgPrintNodesFirst = false;
      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_PRINTSTATE);
   if(iter != cfg->end() )
   {
      cfgPrintState = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_PRINTSPACEINFO);
   if(iter != cfg->end() )
   {
      cfgPrintSpaceInfo = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_PRINTCAPACITYPOOLS);
   if(iter != cfg->end() )
   {
      cfgPrintCapacityPools = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_ERRORCODES);
   if(iter != cfg->end() )
   {
      cfgReportStateErrors = true;
      cfg->erase(iter);
   }

   bool nodeTypeSet;
   cfgNodeType = ModeHelper::nodeTypeFromCfg(cfg, &nodeTypeSet);
   switch (cfgNodeType)
   {
      case NODETYPE_Meta:
         cfgPoolQueryType = CapacityPoolQuery_META;
         break;
      case NODETYPE_Storage:
         cfgPoolQueryType = CapacityPoolQuery_STORAGE;
         break;
      default:
         if (!nodeTypeSet)
         {
            // Default to storage
            cfgNodeType = NODETYPE_Storage;
            cfgPoolQueryType = CapacityPoolQuery_STORAGE;
         }
         else
         {
            std::cerr << "Invalid node type." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
   }

   iter = cfg->find(MODELISTTARGETS_ARG_MIRRORGROUPS);
   if(iter != cfg->end() )
   {
      cfgUseBuddyGroups = true;

      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_FROMMETA);
   if(iter != cfg->end() )
   {
      cfgGetFromMeta = true;

      cfg->erase(iter);

      // getting from meta server requires specified meta nodeID

      if(cfg->empty() )
      {
         std::cerr << "NodeID missing." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      poolSourceNodeID = NumNodeID(StringTk::strToUInt(cfg->begin()->first) );
      cfg->erase(cfg->begin() );
   }

   iter = cfg->find(MODELISTTARGETS_ARG_PRINTSTORAGEPOOLS);
   if(iter != cfg->end() )
   {
      cfgPrintStoragePools = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODELISTTARGETS_ARG_STORAGEPOOL);
   if(iter != cfg->end() )
   {
      if (cfgNodeType != NODETYPE_Storage)
      {
         std::cerr << MODELISTTARGETS_ARG_STORAGEPOOL
                   << " can only be used for storage nodes."
                   << std::endl;
         return APPCODE_INVALID_CONFIG;
      }


      StoragePoolId storagePoolId;
      storagePoolId.fromStr(iter->second);

      cfgStoragePool = Program::getApp()->getStoragePoolStore()->getPool(storagePoolId);

      if (!cfgStoragePool)
      {
         std::cerr << "Given storage pool ID "
                   << iter->second
                   << " can't be resolved to a pool."
                   << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfg->erase(iter);
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   return APPCODE_NO_ERROR;
}

/**
 * Downloads the required data from the management daemon
 *
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::prepareData()
{
   App* app = Program::getApp();

   // set communication source store for pool data
   if(cfgPrintCapacityPools)
   {
      if(cfgGetFromMeta)
         poolSourceNodeStore = app->getMetaNodes();
      else
      {
         poolSourceNodeStore = app->getMgmtNodes();

         auto mgmtNode = poolSourceNodeStore->referenceFirstNode();

         poolSourceNodeID = mgmtNode->getNumID();
      }

      bool getPoolsCommRes = getPoolsComm(poolSourceNodeStore, poolSourceNodeID, cfgPoolQueryType);
      if(!getPoolsCommRes)
      {
         std::cerr << "Download of pools failed." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }

   return APPCODE_NO_ERROR;
}

void ModeListTargets::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --nodesfirst              Print nodes in first column." << std::endl;
   std::cout << "  --longnodes               Print node IDs in long format." << std::endl;
   std::cout << "  --state                   Print state of each target." << std::endl;
   std::cout << "  --errorcodes              Exit code reports an error if a target is not" << std::endl;
   std::cout << "                            in the reachability state online or not in the" << std::endl;
   std::cout << "                            consistency state good, requires the option --state." << std::endl;
   std::cout << "  --pools                   Print the capacity pool which the target belongs to." << std::endl;
   std::cout << "  --nodetype=<nodetype>     The node type to list (meta, storage)." << std::endl;
   std::cout << "                            (Default: storage)" << std::endl;
   std::cout << "  --spaceinfo               Print total/free disk space and available inodes." << std::endl;
   std::cout << "  --mirrorgroups            Print mirror buddy groups instead of targets." << std::endl;
   std::cout << "  --storagepools            Print the storage pool which the target belongs to." << std::endl;
   std::cout << "  --storagepoolid=<poolID>  Print only targets in storage target pool with poolID." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode lists the available storage/meta targets and the nodes to which they " << std::endl;
   std::cout << " are mapped. The mode can also list all servers/targets and their assignment to " << std::endl;
   std::cout << " one of the three capacity pools (\"normal\", \"low\", \"emergency\"), based on " << std::endl;
   std::cout << " the available disk space." << std::endl;
   std::cout << " Pool assignments influence the storage allocation algorithm of metadata" << std::endl;
   std::cout << " servers. Pool limits can be defined in the management server config file." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: List registered storage targets" << std::endl;
   std::cout << "  $ beegfs-ctl --listtargets" << std::endl;
}

/**
 * @param nodes The nodeStore which contains to destination node
 * @param nodeID The nodeID of the destination node (meta or mgmtd) to download the polls
 * @param poolQueryType The query type (CapacityPoolQueryType) for the pool download
 * @return true on success, false on error
 */
bool ModeListTargets::getPoolsComm(NodeStoreServers* nodes, NumNodeID nodeID,
   CapacityPoolQueryType poolQueryType)
{
   auto node = nodes->referenceNode(nodeID);
   if (!node)
   {
      std::cerr << "Unknown node: " << nodeID << std::endl;
      return false;
   }

   bool retVal = false;

   GetNodeCapacityPoolsRespMsg* respMsgCast;

   GetNodeCapacityPoolsMsg msg(poolQueryType);

   const auto respMsg = MessagingTk::requestResponse(*node, msg,
         NETMSGTYPE_GetNodeCapacityPoolsResp);
   if (!respMsg)
   {
      std::cerr << "Pools download failed: " << nodeID << std::endl;
      goto err_cleanup;
   }

   {
      respMsgCast = (GetNodeCapacityPoolsRespMsg*) respMsg.get();

      // for storage nodes we might receive multiple capacity pool map elements (sorted by storage
      // pool here). However, we're totally not interested in the "storage pools" right here, so we
      // just mash them together.
      GetNodeCapacityPoolsRespMsg::PoolsMap& poolsMap = respMsgCast->getPoolsMap();

      for (auto iter = poolsMap.begin(); iter != poolsMap.end(); iter++)
      {
         for(unsigned poolType=0; poolType < CapacityPool_END_DONTUSE; poolType++)
         {
            UInt16List& capacityPools = capacityPoolLists[poolType];
            UInt16List& downloadedCapacityPools = iter->second[poolType];

            capacityPools.splice(capacityPools.end(), downloadedCapacityPools);
         }
      }

      retVal = true;
   }
err_cleanup:
   return retVal;
}

/**
 * prints the the hole table with the targets, with respect to the configuration
 *
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::print()
{
   int retVal = APPCODE_NO_ERROR;

   App* app = Program::getApp();

   printHeader();

   if(cfgUseBuddyGroups && (cfgNodeType == NODETYPE_Storage) )
   {
      FhgfsOpsErr outError = FhgfsOpsErr_SUCCESS;
      NodeStoreServers* storageServers = app->getStorageNodes();
      TargetMapper* targets = app->getTargetMapper();
      MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();

      UInt16List buddyGroupIDs;
      UInt16List buddyGroupPrimaryTargetIDs;
      UInt16List buddyGroupSecondaryTargetIDs;

      buddyGroupMapper->getMappingAsLists(buddyGroupIDs, buddyGroupPrimaryTargetIDs,
         buddyGroupSecondaryTargetIDs);

      for (ZipIterRange<UInt16List, UInt16List, UInt16List>
           iter(buddyGroupIDs, buddyGroupPrimaryTargetIDs, buddyGroupSecondaryTargetIDs);
           !iter.empty(); ++iter)
      {
         uint16_t buddyGroupID = *(iter()->first);
         uint16_t primaryTargetID = *(iter()->second);
         uint16_t secondaryTargetID = *(iter()->third);

         // if a storagePool is set and neither the current primary target nor the secondary target
         // is contained in there, then skip it
         if ( cfgStoragePool &&
              !cfgStoragePool->hasTarget(primaryTargetID) &&
              !cfgStoragePool->hasTarget(secondaryTargetID) )
            continue;

         // print primary target of buddy group
         auto node = storageServers->referenceNodeByTargetID(primaryTargetID, targets, &outError);
         if (!node || outError)
         {
            fprintf(stderr, " [ERROR: Could not find storage servers for targetID: %u]\n",
               primaryTargetID);
            retVal = (retVal == APPCODE_NO_ERROR) ? APPCODE_RUNTIME_ERROR : retVal;
         }
         else
         {
            int err = printTarget(primaryTargetID, true, *node, buddyGroupID, secondaryTargetID);
            retVal = (retVal == APPCODE_NO_ERROR) ? err : retVal;
         }

         // print secondary target of buddy group
         node = storageServers->referenceNodeByTargetID(secondaryTargetID, targets, &outError);
         if(!node || outError)
         {
            fprintf(stderr, " [ERROR: Could not find storage servers for targetID: %u]\n",
               secondaryTargetID);
            retVal = (retVal == APPCODE_NO_ERROR) ? APPCODE_RUNTIME_ERROR : retVal;
         }
         else
         {
            int err = printTarget(secondaryTargetID, false, *node, buddyGroupID, primaryTargetID);
            retVal = (retVal == APPCODE_NO_ERROR) ? err : retVal;
         }
      }
   }
   else
   if(cfgUseBuddyGroups && (cfgNodeType == NODETYPE_Meta) )
   {
      NodeStoreServers* metaServers = app->getMetaNodes();
      MirrorBuddyGroupMapper* buddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

      UInt16List buddyGroupIDs;
      UInt16List buddyGroupPrimaryTargetIDs;
      UInt16List buddyGroupSecondaryTargetIDs;

      buddyGroupMapper->getMappingAsLists(buddyGroupIDs, buddyGroupPrimaryTargetIDs,
         buddyGroupSecondaryTargetIDs);

      for (ZipIterRange<UInt16List, UInt16List, UInt16List>
           groupsIter(buddyGroupIDs, buddyGroupPrimaryTargetIDs, buddyGroupSecondaryTargetIDs);
           !groupsIter.empty(); ++groupsIter)
      {
         uint16_t buddyGroupID = *(groupsIter()->first);
         uint16_t primaryNodeID = *(groupsIter()->second);
         uint16_t secondaryNodeID = *(groupsIter()->third);

         // print primary node of buddy group
         auto node = metaServers->referenceNode(NumNodeID(primaryNodeID) );
         if (!node)
         {
            fprintf(stderr, " [ERROR: Unknown metadata server ID: %u]\n", primaryNodeID);
            retVal = (retVal == APPCODE_NO_ERROR) ? APPCODE_RUNTIME_ERROR : retVal;
         }
         else
         {
            int err = printTarget(primaryNodeID, true, *node, buddyGroupID, secondaryNodeID);
            retVal = (retVal == APPCODE_NO_ERROR) ? err : retVal;
         }

         //print secondary node of buddy group
         node = metaServers->referenceNode(NumNodeID(secondaryNodeID) );
         if (!node)
         {
            fprintf(stderr, " [ERROR: Unknown metadata servcer ID: %u]\n", secondaryNodeID);
            retVal = (retVal == APPCODE_NO_ERROR) ? APPCODE_RUNTIME_ERROR : retVal;
         }
         else
         {
            int err = printTarget(secondaryNodeID, false, *node, buddyGroupID, primaryNodeID);
            retVal = (retVal == APPCODE_NO_ERROR) ? err : retVal;
         }
      }
   }
   else
   if(cfgNodeType == NODETYPE_Storage)
   {
      NodeStoreServers* storageServers = app->getStorageNodes();
      TargetMapper* targets = app->getTargetMapper();

      UInt16List targetIDsList;
      NumNodeIDList nodeIDsList;
      targets->getMappingAsLists(targetIDsList, nodeIDsList);

      for (ZipIterRange<UInt16List, NumNodeIDList> targetNodeIDIter(targetIDsList, nodeIDsList);
           !targetNodeIDIter.empty(); ++targetNodeIDIter)
      {
         // if a storagePool is set and the current target isn't contained in there, then skip it
         if ( cfgStoragePool && !cfgStoragePool->hasTarget(*(targetNodeIDIter()->first)) )
            continue;

         auto node = storageServers->referenceNode(*(targetNodeIDIter()->second));
         if(!node)
         {
            fprintf(stderr, " [ERROR: Unknown storage server ID: %s]\n",
               targetNodeIDIter()->second->str().c_str());
            retVal = (retVal == APPCODE_NO_ERROR) ? APPCODE_RUNTIME_ERROR : retVal;
         }
         else
         {
            int err = printTarget(*(targetNodeIDIter()->first), false, *node, 0, 0);
            retVal = (retVal == APPCODE_NO_ERROR) ? err : retVal;
         }
      }
   }
   else
   if(cfgNodeType == NODETYPE_Meta)
   {
      NodeStoreServers* metaServers = app->getMetaNodes();
      const auto nodes = metaServers->referenceAllNodes();
      if (nodes.empty())
      {
         fprintf(stderr, " [ERROR: No metadata servers found]\n");
         retVal = (retVal == APPCODE_NO_ERROR) ? APPCODE_RUNTIME_ERROR : retVal;
      }

      for (const auto& node : nodes)
      {
         int err = printTarget(node->getNumID().val(), false, *node, 0, 0);
         retVal = (retVal == APPCODE_NO_ERROR) ? err : retVal;
      }
   }

   return retVal;
}

/**
 * prints the header of the table to the console
 *
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::printHeader()
{
   char* description = (char*) malloc(MODELISTTARGETS_MAX_LINE_LENGTH);
   char* spacer = (char*) malloc(MODELISTTARGETS_MAX_LINE_LENGTH);

   int sizeDescription = 0;
   int sizeSpacer = 0;

   if(cfgPrintNodesFirst && !cfgPrintLongNodes && !cfgHideNodeID)
   {
      sizeDescription += sprintf(&description[sizeDescription], "%8s ",
         "NodeID");
      sizeSpacer += sprintf(&spacer[sizeSpacer], "%8s ",
         "======");
   }

   if(cfgUseBuddyGroups)
   {
      sizeDescription += sprintf(&description[sizeDescription], "%12s %12s ",
         "MirrorGroupID", "MGMemberType");
      sizeSpacer += sprintf(&spacer[sizeSpacer], "%12s %12s ",
         "=============", "============");
   }

   sizeDescription += sprintf(&description[sizeDescription], "%8s ",
      "TargetID");
   sizeSpacer += sprintf(&spacer[sizeSpacer], "%8s ",
      "========");

   if(cfgPrintState)
   {
      sizeDescription += sprintf(&description[sizeDescription], "%16s %12s ",
         "Reachability", "Consistency");
      sizeSpacer += sprintf(&spacer[sizeSpacer], "%16s %12s ",
         "============", "===========");
   }

   if(cfgPrintStoragePools)
   {
      sizeDescription += sprintf(&description[sizeDescription], "%10s ",
         "St. Pool");
      sizeSpacer += sprintf(&spacer[sizeSpacer], "%10s ",
         "========");
   }

   if(cfgPrintCapacityPools)
   {
      sizeDescription += sprintf(&description[sizeDescription], "%11s ",
         "Cap. Pool");
      sizeSpacer += sprintf(&spacer[sizeSpacer], "%11s ",
         "=========");
   }

   if(cfgPrintSpaceInfo)
   {
      sizeDescription += sprintf(&description[sizeDescription], "%12s %12s %4s %11s %11s %4s ",
         "Total", "Free", "%", "ITotal", "IFree", "%");
      sizeSpacer += sprintf(&spacer[sizeSpacer], "%12s %12s %4s %11s %11s %4s ",
         "=====", "====", "=", "======", "=====", "=");
   }

   if( (!cfgPrintNodesFirst || cfgPrintLongNodes) && !cfgHideNodeID)
   {
      sizeDescription += sprintf(&description[sizeDescription], "%8s ",
         "NodeID");
      sizeSpacer += sprintf(&spacer[sizeSpacer], "%8s ",
         "======");
   }

   // Overwrite last "space" character with a newline.
   description[sizeDescription - 1] = '\n';
   spacer[sizeSpacer - 1] = '\n';

   printf("%s", description);
   printf("%s", spacer);

   SAFE_FREE(description);
   SAFE_FREE(spacer);

   return APPCODE_NO_ERROR;
}

/**
 * prints one line of the table; the information of one target
 *
 * @param targetID The targetNumID of the target to print
 * @param isPrimaryTarget true if this target is the primary target of the buddy group
 * @param node the node where the target is located, must be referenced before given to this method
 * @param buddyGroupID the buddy group ID where the target belongs to
 * @param buddyTargetID the targetNumID of the buddy target
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::printTarget(uint16_t targetID, bool isPrimaryTarget, Node& node,
   uint16_t buddyGroupID, uint16_t buddyTargetID)
{
   int retVal = APPCODE_NO_ERROR;

   char* lineString = (char*) malloc(MODELISTTARGETS_MAX_LINE_LENGTH);
   int sizeLine = 0;

   // print long node IDs forbids --nodesfirst
   if(cfgPrintNodesFirst && !cfgPrintLongNodes && !cfgHideNodeID)
      addNodeIDToLine(lineString, &sizeLine, node);

   if(cfgUseBuddyGroups)
      addBuddyGroupToLine(lineString, &sizeLine, buddyGroupID, isPrimaryTarget);

   addTargetIDToLine(lineString, &sizeLine, targetID);

   if(cfgPrintState)
   {
      int stateRetVal = addStateToLine(lineString, &sizeLine, targetID, buddyTargetID);
      retVal = (retVal == APPCODE_NO_ERROR) ? stateRetVal : retVal;
   }

   if(cfgPrintStoragePools)
      retVal = (retVal == APPCODE_NO_ERROR) ?
         addStoragePoolToLine(lineString, &sizeLine, targetID) : retVal;

   if(cfgPrintCapacityPools)
      retVal = (retVal == APPCODE_NO_ERROR) ?
         addCapacityPoolToLine(lineString, &sizeLine, targetID) : retVal;

   if(cfgPrintSpaceInfo)
      retVal = (retVal == APPCODE_NO_ERROR) ?
         addSpaceToLine(lineString, &sizeLine, targetID, node) : retVal;

   if( (!cfgPrintNodesFirst || cfgPrintLongNodes) && !cfgHideNodeID)
      addNodeIDToLine(lineString, &sizeLine, node);

   // Overwrite last "space" character with a newline (so we don't have a trailing space)
   lineString[sizeLine - 1] = '\n';
   printf("%s", lineString);

   SAFE_FREE(lineString);

   return retVal;
}

/**
 * add the targetNumID to the output string of a table line
 *
 * @param inOutString the output string of a target for the output table
 * @param inOutOffset the offset in the inOutString to start with the targetNumID
 * @param currentTargetID the targetNumID to print
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::addTargetIDToLine(char* inOutString, int* inOutOffset,
   uint16_t currentTargetID)
{
   *inOutOffset += sprintf(&inOutString[*inOutOffset],"%8hu ", currentTargetID);

   return APPCODE_NO_ERROR;
}

/**
 * add the nodeNumID to the output string of a table line
 *
 * @param inOutString the output string of a target for the output table
 * @param inOutOffset the offset in the inOutString to start with the nodeNumID
 * @param node the node where the target is located, must be referenced before given to this method
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::addNodeIDToLine(char* inOutString, int* inOutOffset, Node& node)
{
   if(cfgPrintLongNodes) // note: two extra spaces to compensate for %8s
      *inOutOffset += sprintf(&inOutString[*inOutOffset], "  %s ",
         node.getNodeIDWithTypeStr().c_str() );
   else
      *inOutOffset += sprintf(&inOutString[*inOutOffset], "%8s ", node.getNumID().str().c_str() );

   return APPCODE_NO_ERROR;
}

/**
 * add the state of a target to the output string of a table line
 *
 * @param inOutString the output string of a target for the output table
 * @param inOutOffset the offset in the inOutString to start with the state of the target
 * @param currentTargetID the targetNumID of the target where the state is needed
 * @param buddyTargetID the targetNumID of the buddy target
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::addStateToLine(char* inOutString, int* inOutOffset, uint16_t currentTargetID,
   uint16_t buddyTargetID)
{
   int retVal = APPCODE_NO_ERROR;

   App* app = Program::getApp();
   TargetStateStore* targetStateStore;
   MirrorBuddyGroupMapper* buddyGroupMapper;

   CombinedTargetState state;

   if (cfgNodeType == NODETYPE_Storage)
   {
      targetStateStore = app->getTargetStateStore();
      buddyGroupMapper = app->getMirrorBuddyGroupMapper();
   }
   else
   {
      targetStateStore = app->getMetaTargetStateStore();
      buddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();
   }

   if (!targetStateStore->getState(currentTargetID, state) )
   {
      *inOutOffset += sprintf(&inOutString[*inOutOffset], "%29s ", "<invalid_value>");

      if(cfgReportStateErrors)
         retVal = APPCODE_RUNTIME_ERROR;
   }
   else
   {
      if(cfgReportStateErrors)
      {
         if (state.consistencyState != TargetConsistencyState_GOOD)
            retVal = APPCODE_TARGET_NOT_GOOD;
         else if (state.reachabilityState != TargetReachabilityState_ONLINE)
            retVal = APPCODE_TARGET_NOT_ONLINE;
      }

      std::string consistencyStateString = TargetStateStore::stateToStr(state.consistencyState);

      if(state.consistencyState == TargetConsistencyState_NEEDS_RESYNC)
      {
         // find the buddy target ID, this is required when the option --mirrorgroups is not used
         if(!buddyTargetID)
         {
            UInt16List buddyGroupIDs;
            UInt16List buddyGroupPrimaryTargetIDs;
            UInt16List buddyGroupSecondaryTargetIDs;
            buddyGroupMapper->getMappingAsLists(buddyGroupIDs, buddyGroupPrimaryTargetIDs,
               buddyGroupSecondaryTargetIDs);

            for (ZipIterRange<UInt16List, UInt16List>
                 buddyGroupsIter(buddyGroupPrimaryTargetIDs, buddyGroupSecondaryTargetIDs);
                 !buddyGroupsIter.empty(); ++buddyGroupsIter)
            {
               if(*(buddyGroupsIter()->first) == currentTargetID)
               {
                  buddyTargetID = *(buddyGroupsIter()->second);
                  break;
               }
               else
               if(*(buddyGroupsIter()->second) == currentTargetID)
               {
                  buddyTargetID = *(buddyGroupsIter()->first);
                  break;
               }
            }
         }

         if (cfgNodeType == NODETYPE_Storage)
         {
            StorageBuddyResyncJobStatistics jobStats;
            ModeHelper::getStorageBuddyResyncStats(buddyTargetID, jobStats);

            if (jobStats.getState() == BuddyResyncJobState_RUNNING)
               consistencyStateString = MODELISTTARGETS_TARGET_CONSISTENCY_STATE_RESYNCING;
         }
         else
         {
            MetaBuddyResyncJobStatistics jobStats;
            ModeHelper::getMetaBuddyResyncStats(buddyTargetID, jobStats);

            if (jobStats.getState() == BuddyResyncJobState_RUNNING)
               consistencyStateString = MODELISTTARGETS_TARGET_CONSISTENCY_STATE_RESYNCING;
         }
      }

      *inOutOffset += sprintf(&inOutString[*inOutOffset], "%16s %12s ",
         TargetStateStore::stateToStr(state.reachabilityState), consistencyStateString.c_str() );
   }

   return retVal;
}

/**
 * add the capacity pool where the target belongs to to the output string of a table line
 *
 * @param inOutString the output string of a target for the output table
 * @param inOutOffset the offset in the inOutString to start with the pool of the target
 * @param currentTargetID the targetNumID of the target where the pool is needed
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::addCapacityPoolToLine(char* inOutString, int* inOutOffset,
   uint16_t currentTargetID)
{
   int retVal = APPCODE_NO_ERROR;

   std::string poolName;

   const UInt16List& poolNormalList = capacityPoolLists[CapacityPool_NORMAL];
   const UInt16List& poolLowList = capacityPoolLists[CapacityPool_LOW];
   const UInt16List& poolEmergencyList = capacityPoolLists[CapacityPool_EMERGENCY];

   if(std::find(poolNormalList.begin(), poolNormalList.end(), currentTargetID)
         != poolNormalList.end() )
   {
      poolName = MODELISTTARGETS_POOL_NORMAL;
   }
   else if(std::find(poolLowList.begin(), poolLowList.end(), currentTargetID) != poolLowList.end())
   {
      poolName = MODELISTTARGETS_POOL_LOW;
   }
   else if(std::find(poolEmergencyList.begin(), poolEmergencyList.end(), currentTargetID)
           != poolEmergencyList.end())
   {
      poolName = MODELISTTARGETS_POOL_EMERGENCY;
   }
   else
   {
      poolName = MODELISTTARGETS_POOL_UNKNOWN;
      retVal = APPCODE_RUNTIME_ERROR;
   }

   *inOutOffset += sprintf(&inOutString[*inOutOffset], "%11s ", poolName.c_str() );

   return retVal;
}

/**
 * add the space/inode information of the target to the output string of a table line
 *
 * @param inOutString the output string of a target for the output table
 * @param inOutOffset the offset in the inOutString to start with the space/inode information
 * @param currentTargetID the targetNumID of the target where the space/inode information is needed
 * @param node the node where the target is located, must be referenced before given to this method
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::addSpaceToLine(char* inOutString, int* inOutOffset, uint16_t currentTargetID,
   Node& node)
{
   // print total/free space information

   // get and print space info

   FhgfsOpsErr statRes;
   int64_t freeSpace = 0;
   int64_t totalSpace = 0;
   double freeSpaceGiB;
   double totalSpaceGiB;
   int64_t freeInodes = 0;
   int64_t totalInodes = 0;
   double freeInodesM; // m=mega (as in mega byte)
   double totalInodesM; // m=mega (as in mega byte)
   double freeSpacePercent;
   double freeInodesPercent;
   double gb = 1024*1024*1024;
   double m = 1000*1000;

   // retrieve space info from server

   statRes = StorageTargetInfo::statStoragePath(node, currentTargetID, &freeSpace, &totalSpace,
      &freeInodes, &totalInodes);
   if(statRes != FhgfsOpsErr_SUCCESS)
   {
      // don't log to stderr directly, because that would distort console output if server down
      fprintf(stderr, " [ERROR from %s: %s]\n", node.getNodeIDWithTypeStr().c_str(),
         boost::lexical_cast<std::string>(statRes).c_str());
   }

   // make values human readable

   freeSpaceGiB = freeSpace / gb;
   totalSpaceGiB = totalSpace / gb;
   freeSpacePercent = totalSpace ? (100.0 * freeSpace / totalSpace) : 0;
   freeInodesM = freeInodes / m;
   totalInodesM = totalInodes / m;
   freeInodesPercent = totalInodes ? (100.0 * freeInodes / totalInodes) : 0;

   *inOutOffset += sprintf(&inOutString[*inOutOffset],
      "%9.1fGiB %9.1fGiB %3.0f%% %10.1fM %10.1fM %3.0f%% ",
      (float)totalSpaceGiB, (float)freeSpaceGiB, (float)freeSpacePercent,
      (float)totalInodesM, (float)freeInodesM, (float)freeInodesPercent);

   return (statRes == FhgfsOpsErr_SUCCESS) ? APPCODE_NO_ERROR : APPCODE_RUNTIME_ERROR;
}

/**
 * add the space/inode information of the target to the output string of a table line
 *
 * @param inOutString the output string of a target for the output table
 * @param inOutOffset the offset in the inOutString to start with the space/inode information
 * @param currentBuddyGroupID the buddy group ID where the target belongs to
 * @param isPrimaryTarget true if this target is the primary target of the buddy group
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::addBuddyGroupToLine(char* inOutString, int* inOutOffset,
   uint16_t currentBuddyGroupID, bool isPrimaryTarget)
{
   *inOutOffset += sprintf(&inOutString[*inOutOffset], "%13hu %12s ", currentBuddyGroupID,
      isPrimaryTarget ? MODELISTTARGETS_BUDDYGROUP_MEMBERTYPE_PRIMARY :
         MODELISTTARGETS_BUDDYGROUP_MEMBERTYPE_SECONDARY);

   return APPCODE_NO_ERROR;
}

/**
 * add the storage pool where the target belongs to to the output string of a table line
 *
 * @param inOutString the output string of a target for the output table
 * @param inOutOffset the offset in the inOutString to start with the pool of the target
 * @param currentTargetID the targetNumID of the target where the pool is needed
 * @return 0 on success, all other values reports an error (APPCODE_...)
 */
int ModeListTargets::addStoragePoolToLine(char* inOutString, int* inOutOffset,
   uint16_t currentTargetID)
{
   const StoragePoolPtrVec pools = Program::getApp()->getStoragePoolStore()->getPoolsAsVec();

   for (StoragePoolPtrVecCIter iter = pools.begin(); iter != pools.end(); iter++)
   {
      if ((*iter)->hasTarget(currentTargetID))
      {
         StoragePoolId poolId = (*iter)->getId();

         *inOutOffset += sprintf(&inOutString[*inOutOffset], "%10s ", poolId.str().c_str() );
         return APPCODE_NO_ERROR;
      }
   }

   // no pool found => error
   *inOutOffset += sprintf(&inOutString[*inOutOffset], "%10s ", MODELISTTARGETS_POOL_UNKNOWN );

   return APPCODE_RUNTIME_ERROR;
}
