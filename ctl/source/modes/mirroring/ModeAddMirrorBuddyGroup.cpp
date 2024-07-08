#include <app/App.h>
#include <common/nodes/NodeStore.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>

#include "ModeAddMirrorBuddyGroup.h"



#define MODEADDMIRRORBUDDYGROUP_ARG_PRIMARY      "--primary"
#define MODEADDMIRRORBUDDYGROUP_ARG_SECONDARY    "--secondary"
#define MODEADDMIRRORBUDDYGROUP_ARG_GROUPID      "--groupid"
#define MODEADDMIRRORBUDDYGROUP_ARG_MGROUPID     "--mirrorgroupid"
#define MODEADDMIRRORBUDDYGROUP_ARG_AUTOMATIC    "--automatic"
#define MODEADDMIRRORBUDDYGROUP_ARG_FORCE        "--force"
#define MODEADDMIRRORBUDDYGROUP_ARG_DRYRUN       "--dryrun"
#define MODEADDMIRRORBUDDYGROUP_ARG_UNIQUE_ID    "--uniquegroupid"


int ModeAddMirrorBuddyGroup::execute()
{
   const unsigned mgmtTimeoutMS = 2500;
   const unsigned mgmtNameResolutionRetries = 3;

   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();

   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   uint16_t cfgPrimaryTargetID = 0;
   uint16_t cfgSecondaryTargetID = 0;
   uint16_t cfgGroupID = 0;

   ModeHelper::printEnterpriseFeatureMsg();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   StringMapIter iter;

   // check if automatic mode is selected
   iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_AUTOMATIC);
   if(iter != cfg->end() )
   {
      this->cfgAutomatic = true;
      cfg->erase(iter);
   }

   // check if the automatic mode should ignore warnings
   iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_FORCE);
   if(iter != cfg->end() )
   {
      this->cfgForce = true;
      cfg->erase(iter);
   }

   // check if the automatic mode should only print the selected configuration
   iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_DRYRUN);
   if(iter != cfg->end() )
   {
      this->cfgDryrun = true;
      cfg->erase(iter);
   }

   /* check if the automatic mode should use unique MirrorBuddyGroupIDs.
   The ID is not used as storage targetNumID */
   iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_UNIQUE_ID);
   if(iter != cfg->end() )
   {
      this->cfgUnique = true;
      cfg->erase(iter);
   }

   this->nodeType = ModeHelper::nodeTypeFromCfg(cfg);
   switch (this->nodeType)
   {
      case NODETYPE_Storage:
         this->nodes = app->getStorageNodes();
         break;

      case NODETYPE_Meta:
         this->nodes = app->getMetaNodes();
         break;

      default:
         std::cerr << "Invalid or missing node type." << std::endl;
         return APPCODE_INVALID_CONFIG;
   }

   if(this->cfgAutomatic)
   { // sanity checks of automatic mode parameters
      if(this->cfgForce && this->cfgDryrun)
      {
         std::cerr << "Invalid arguments: Only one of " << MODEADDMIRRORBUDDYGROUP_ARG_FORCE <<
            " and " << MODEADDMIRRORBUDDYGROUP_ARG_DRYRUN << " is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }
   else
   {
      if(this->cfgForce)
      { // sanity checks of automatic mode parameter
         std::cerr << "Invalid argument: "<< MODEADDMIRRORBUDDYGROUP_ARG_FORCE <<
            " is not allowed without " << MODEADDMIRRORBUDDYGROUP_ARG_AUTOMATIC << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if(this->cfgDryrun)
      { // sanity checks of automatic mode parameter
         std::cerr << "Invalid argument: "<< MODEADDMIRRORBUDDYGROUP_ARG_DRYRUN <<
            " is not allowed without " << MODEADDMIRRORBUDDYGROUP_ARG_AUTOMATIC << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if(this->cfgUnique)
      { // sanity checks of automatic mode parameter
         std::cerr << "Invalid argument: "<< MODEADDMIRRORBUDDYGROUP_ARG_UNIQUE_ID <<
            " is not allowed without " << MODEADDMIRRORBUDDYGROUP_ARG_AUTOMATIC << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_PRIMARY);
      if(iter != cfg->end())
      { // found a primary target ID
         bool isNumericRes = StringTk::isNumeric(iter->second);
         if(!isNumericRes)
         {
            std::cerr << "Invalid primary target ID given (must be numeric): " << iter->second
               << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         cfgPrimaryTargetID = StringTk::strToUInt(iter->second);
         cfg->erase(iter);
      }
      else
      { // primary target ID is mandatory but wasn't found
         std::cerr << "No primary target ID specified." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_SECONDARY);
      if(iter != cfg->end())
      { // found a secomdary target ID
         bool isNumericRes = StringTk::isNumeric(iter->second);
         if(!isNumericRes)
         {
            std::cerr << "Invalid secondary target ID given (must be numeric): " << iter->second
               << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         cfgSecondaryTargetID = StringTk::strToUInt(iter->second);
         cfg->erase(iter);
      }
      else
      { // secondary target ID is mandatory but wasn't found
         std::cerr << "No secondary target ID specified." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if (cfgPrimaryTargetID == cfgSecondaryTargetID)
      {
         std::cerr << "Primary and secondary target must be different." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if (cfg->count(MODEADDMIRRORBUDDYGROUP_ARG_GROUPID) > 0 &&
            cfg->count(MODEADDMIRRORBUDDYGROUP_ARG_MGROUPID) > 0)
      {
         std::cerr << MODEADDMIRRORBUDDYGROUP_ARG_GROUPID << " and "
            << MODEADDMIRRORBUDDYGROUP_ARG_MGROUPID << " are mutually exclusive." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgGroupID = 0;

      iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_GROUPID);
      if(iter != cfg->end() )
      { // found a group ID
         bool isNumericRes = StringTk::isNumeric(iter->second);
         if(!isNumericRes)
         {
            std::cerr << "Invalid mirror buddy group ID given (must be numeric): " << iter->second
               << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         cfgGroupID = StringTk::strToUInt(iter->second);
         cfg->erase(iter);
      }

      iter = cfg->find(MODEADDMIRRORBUDDYGROUP_ARG_MGROUPID);
      if(iter != cfg->end() )
      { // found a group ID
         bool isNumericRes = StringTk::isNumeric(iter->second);
         if(!isNumericRes)
         {
            std::cerr << "Invalid mirror buddy group ID given (must be numeric): " << iter->second
               << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         cfgGroupID = StringTk::strToUInt(iter->second);
         cfg->erase(iter);
      }
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   // check mgmt node
   if(!NodesTk::waitForMgmtHeartbeat(
            NULL, dgramLis, this->mgmtNodes, mgmtHost, mgmtPortUDP, mgmtTimeoutMS,
            mgmtNameResolutionRetries))
   {
      std::cerr << "Management node communication failed: " << mgmtHost << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if(this->cfgAutomatic)
   {
      return doAutomaticMode();
   }
   else
   {
      if (nodeType == NODETYPE_Storage)
      {
         // for storage groups, check first if targets are in the same storage pool
         StoragePoolPtr storagePool = storagePoolStore->getPool(cfgPrimaryTargetID);

         if (!storagePool)
         {
            // should actually never happen
            std::cerr << "Could not add buddy group: "
               "Requested primary target is not a member of a storage pool." << std::endl;
            return APPCODE_RUNTIME_ERROR;
         }

         bool haveSamePool = storagePool->hasTarget(cfgSecondaryTargetID);

         if (!haveSamePool)
         {
            std::cerr
               << "Could not add buddy group: Requested targets are in different storage pools."
               << std::endl;
            return APPCODE_RUNTIME_ERROR;
         }
      }

      auto addRes = addGroup(cfgPrimaryTargetID, cfgSecondaryTargetID, cfgGroupID);
      if (addRes == FhgfsOpsErr_NOTOWNER && nodeType == NODETYPE_Meta)
      {
         std::cerr << "Could not add buddy group: new group would own the root inode, but the root\n"
            "inode is owned by the secondary of the new group. Only the primary of a\n"
            "new group may own the root inode; try switching primary and secondary.\n";
         return APPCODE_RUNTIME_ERROR;
      }
      else if (addRes != FhgfsOpsErr_SUCCESS)
      {
         std::cerr << "Could not add buddy group: " << addRes << "\n";
         return APPCODE_RUNTIME_ERROR;
      }
      else
         return APPCODE_NO_ERROR;
   }
}

void ModeAddMirrorBuddyGroup::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory for automatic group creation:" << std::endl;
   std::cout << "  --automatic             Generates the mirror buddy groups automatically with" << std::endl;
   std::cout << "                          respect to the location of the targets. This mode" << std::endl;
   std::cout << "                          aborts the creation of the mirror buddy groups if some" << std::endl;
   std::cout << "                          constraints are not fulfilled." << std::endl;
   std::cout << "  --nodetype=<nodetype>   The node type (metadata, storage)." << std::endl;
   std::cout << std::endl;
   std::cout << " Mandatory for manual group creation:" << std::endl;
   std::cout << "  --primary=<targetID>    The ID of the primary target for this mirror buddy" << std::endl;
   std::cout << "                          group." << std::endl;
   std::cout << "  --secondary=<targetID>  The ID of the secondary target for this mirror buddy" << std::endl;
   std::cout << "                          group." << std::endl;
   std::cout << "  --nodetype=<nodetype>   The node type (metadata, storage)." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional for automatic group creation:" << std::endl;
   std::cout << "  --force                 Ignore the warnings of the automatic mode." << std::endl;
   std::cout << "  --dryrun                Only print the selected configuration of the" << std::endl;
   std::cout << "                          mirror buddy groups." << std::endl;
   std::cout << "  --uniquegroupid         Use unique mirror buddy group IDs, distinct from" << std::endl;
   std::cout << "                          storage target IDs." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional for manual group creation:" << std::endl;
   std::cout << "  --mirrorgroupid=<groupID>" << std::endl;
   std::cout << "  --groupid=<groupID>     Use the given ID for the new buddy group." << std::endl;
   std::cout << "                          Only one of the two options may given." << std::endl;
   std::cout << "                          (Valid range: 1..65535)" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode adds a mirror buddy group and maps two storage targets to the group." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Automatically generate mirror buddy groups from all storage targets" << std::endl;
   std::cout << "  # beegfs-ctl --addmirrorgroup --automatic --nodetype=storage" << std::endl;
}

/**
 * Creates the MirrorBuddyGroups without user interaction
 *
 * @return FhgfsOpsErr...
 */
FhgfsOpsErr ModeAddMirrorBuddyGroup::doAutomaticMode()
{
   if (this->nodeType != NODETYPE_Meta && this->nodeType != NODETYPE_Storage)
   {
      std::cerr << "Only available for metadata and storage nodes." << std::endl;
      return FhgfsOpsErr_INTERNAL;
   }

   std::vector<NodeHandle> nodesList;

   auto mgmtNode = this->mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
   {
      std::cerr << "Failed to reference management node." << std::endl;
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   //download the list of servers from the management node
   if(!NodesTk::downloadNodes(*mgmtNode, this->nodeType, nodesList, false, NULL))
   {
      std::cerr << "Node download failed." << std::endl;
      return FhgfsOpsErr_COMMUNICATION;
   }

   NodesTk::moveNodesFromListToStore(nodesList, nodes);

   if (this->nodeType == NODETYPE_Storage)
   {
      auto mappings = NodesTk::downloadTargetMappings(*mgmtNode, false);
      if (!mappings.first)
      {
         std::cerr << "Target mappings download failed." << std::endl;
         return FhgfsOpsErr_COMMUNICATION;
      }

      systemTargetMapper.syncTargets(mappings.second);
      localTargetMapper.syncTargets(std::move(mappings.second));
   }
   else
   {
      std::vector<NodeHandle> nodeList;

      if (!NodesTk::downloadNodes(*mgmtNode, this->nodeType, nodeList, false, NULL) )
      {
         std::cerr << "Node IDs download failed." << std::endl;
         return FhgfsOpsErr_COMMUNICATION;
      }

      for (auto nodeIter = nodeList.begin(); nodeIter != nodeList.end(); ++nodeIter)
      {
         NumNodeID nodeID = (*nodeIter)->getNumID();
         // note: pool ID not relevant for MDS mappings
         this->localTargetMapper.mapTarget(nodeID.val(), nodeID, StoragePoolStore::INVALID_POOL_ID);
         this->systemTargetMapper.mapTarget(nodeID.val(), nodeID,
                                            StoragePoolStore::INVALID_POOL_ID);
      }
   }

   // Download mirror buddy groups
   UInt16List oldBuddyGroupIDs;
   UInt16List oldPrimaryIDs;
   UInt16List oldSecondaryIDs;

   if(!NodesTk::downloadMirrorBuddyGroups(*mgmtNode, this->nodeType, &oldBuddyGroupIDs,
      &oldPrimaryIDs, &oldSecondaryIDs, false) )
   {
      std::cerr << "Download of existing mirror buddy groups failed." << std::endl;
      return FhgfsOpsErr_COMMUNICATION;
   }

   // Remove all targets / nodes that already belong to a group from our list / targetmapper.
   removeTargetsFromExistingMirrorBuddyGroups(&oldPrimaryIDs, &oldSecondaryIDs);

   UInt16List buddyGroupIDs;
   MirrorBuddyGroupList buddyGroups;

   FhgfsOpsErr retVal;

   retVal = generateMirrorBuddyGroups(&buddyGroupIDs, &buddyGroups, &oldBuddyGroupIDs);

   printAutomaticResults(retVal, &buddyGroupIDs, &buddyGroups, &oldBuddyGroupIDs,
      &oldPrimaryIDs, &oldSecondaryIDs);

   bool retValCreateGroups = createMirrorBuddyGroups(retVal, &buddyGroupIDs, &buddyGroups);

   if(!retValCreateGroups)
      std::cerr << "An error occurred during mirror buddy group creation. "
         "It is possible that some mirror buddy groups have been created anyways. "
         "Please check the messages printed above."
         << std::endl;

   // if used with --force, return success if retVal signals invalid values
   if(retVal == FhgfsOpsErr_INVAL && cfgForce)
      return FhgfsOpsErr_SUCCESS;

   return retVal;
}

/**
 * Prints the given MirrorBuddyGroups on the console
 *
 * @param buddyGroupIDs A list with the MirrorBuddyGroupIDs to print
 * @param primaryIDs A list with the Target/NodeNumIDs of the primary targes/nodees to print
 * @param secondaryIDs List with the Target/NodeNumIDs of the secondary targets/nodes to print
 */
void ModeAddMirrorBuddyGroup::printMirrorBuddyGroups(UInt16List* buddyGroupIDs,
   UInt16List* primaryIDs, UInt16List* secondaryIDs)
{
   MirrorBuddyGroupList buddyGroups;

   for (ZipIterRange<UInt16List, UInt16List> iter(*primaryIDs, *secondaryIDs);
        !iter.empty(); ++iter)
   {
      buddyGroups.push_back(MirrorBuddyGroup(*(iter()->first), *(iter()->second)) );
   }

   printMirrorBuddyGroups(buddyGroupIDs, &buddyGroups);
}

/**
 * Prints the given MirrorBuddyGroups on the console
 *
 * @param buddyGroupIDs A list with all MirrorBuddyGroupIDs to print
 * @param buddyGroups A list with all MirrorBuddyGroups to print
 */
void ModeAddMirrorBuddyGroup::printMirrorBuddyGroups(UInt16List* buddyGroupIDs,
   MirrorBuddyGroupList* buddyGroups)
{
   if (this->nodeType == NODETYPE_Meta)
   {
      printf("%12s %11s %s\n", "BuddyGroupID", "Node type", "Node");
      printf("%12s %11s %s\n", "============", "=========", "====");
   }
   else
   {
      printf("%12s %11s %s\n", "BuddyGroupID", "Target type", "Target");
      printf("%12s %11s %s\n", "============", "===========", "======");
   }


   for (ZipIterRange<UInt16List, MirrorBuddyGroupList> iter(*buddyGroupIDs, *buddyGroups);
        !iter.empty(); ++iter)
   {
      NodeHandle firstNode;
      NodeHandle secondNode;

      if (this->nodeType == NODETYPE_Storage)
      {
         firstNode = nodes->referenceNodeByTargetID( (iter()->second)->firstTargetID,
            &this->systemTargetMapper);
         secondNode =
            nodes->referenceNodeByTargetID( (iter()->second)->secondTargetID,
               &this->systemTargetMapper);
      }
      else
      {
         firstNode = nodes->referenceNode(NumNodeID(iter()->second->firstTargetID) );
         secondNode = nodes->referenceNode(NumNodeID(iter()->second->secondTargetID) );
      }

      std::string firstTarget = StringTk::uintToStr( (iter()->second)->firstTargetID);
      std::string secondTarget = StringTk::uintToStr( (iter()->second)->secondTargetID);

      printf("%12hu %11s %8s @ %s\n", *(iter()->first), "primary", firstTarget.c_str(),
         firstNode->getNodeIDWithTypeStr().c_str() );
      printf("%12s %11s %8s @ %s\n", "", "secondary", secondTarget.c_str(),
         secondNode->getNodeIDWithTypeStr().c_str() );
   }
}

/**
 * Prints the results of the automatic mode to the console.
 *
 * @param retValGeneration the return value of the MirrorBuddyGroup generation
 * @param newBuddyGroupIDs A list with all new MirrorBuddyGroupIDs to print
 * @param newBuddyGroups A list with all new MirrorBuddyGroups to print
 * @param oldBuddyGroupIDs A list with the MirrorBuddyGroupIDs of existing MirrorBuddyGroups to
 *        print
 * @param oldPrimaryIDs A list with the Target/NodeNumIDs of the primary target of existing
 *        MirrorBuddyGroups to print
 * @param oldSecondaryIDs A list with the Target/NodeNumIDs of the secondary target of existing
 *        MirrorBuddyGroups to print
 */
void ModeAddMirrorBuddyGroup::printAutomaticResults(FhgfsOpsErr retValGeneration,
   UInt16List* newBuddyGroupIDs, MirrorBuddyGroupList* newBuddyGroups, UInt16List* oldBuddyGroupIDs,
   UInt16List* oldPrimaryIDs, UInt16List* oldSecondaryIDs)
{
   std::cout << std::endl;
   if (!oldBuddyGroupIDs->empty())
   {
      std::cout << "Existing mirror groups:" << std::endl;
      printMirrorBuddyGroups(oldBuddyGroupIDs, oldPrimaryIDs, oldSecondaryIDs);
      std::cout << std::endl;
   }

   std::cout << "New mirror groups:" << std::endl;
   printMirrorBuddyGroups(newBuddyGroupIDs, newBuddyGroups);
   std::cout << std::endl;

   if( (retValGeneration == FhgfsOpsErr_INVAL) && !this->cfgForce)
   { // abort if a warning occurred and not the force parameter is given
      std::cerr << "Some issues occurred during the mirror buddy group generation. "
         "No mirror buddy groups were created. "
         "Check the warnings printed above, and restart the command "
         "with --force to ignore these warnings." << std::endl;
   }
   else
   if( (retValGeneration != FhgfsOpsErr_SUCCESS) && (retValGeneration != FhgfsOpsErr_INVAL) )
   {
      std::cerr <<
         "An error occurred during mirror group generation. No mirror groups were created."
         << std::endl;
   }
}
