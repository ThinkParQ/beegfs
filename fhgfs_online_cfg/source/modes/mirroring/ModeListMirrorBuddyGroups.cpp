#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/ZipIterator.h>

#include <program/Program.h>

#include "ModeListMirrorBuddyGroups.h"

int ModeListMirrorBuddyGroups::execute()
{
   const int mgmtTimeoutMS = 2500;

   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg);

   if (nodeType != NODETYPE_Meta && nodeType != NODETYPE_Storage)
   {
      std::cerr << "Invalid or missing node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   // check mgmt node

   if(!NodesTk::waitForMgmtHeartbeat(
      NULL, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP, mgmtTimeoutMS) )
   {
      std::cerr << "Management node communication failed: " << mgmtHost << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   // download buddy groups

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   if(!NodesTk::downloadMirrorBuddyGroups(*mgmtNode, nodeType, &buddyGroupIDs,
      &primaryTargetIDs, &secondaryTargetIDs, false) )
   {
      std::cerr << "Download of mirror buddy groups failed." << std::endl;
      retVal = APPCODE_RUNTIME_ERROR;
      goto cleanup_mgmt;
   }

   // print results
   printGroups(nodeType, buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs);

   retVal = APPCODE_NO_ERROR;

cleanup_mgmt:
   return retVal;
}

void ModeListMirrorBuddyGroups::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --nodetype=<nodetype>  The node type (metadata, storage)." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode lists the available mirror buddy groups with their mapped targets." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: List registered storage target mirror buddy groups" << std::endl;
   std::cout << "  $ beegfs-ctl --listmirrorgroups --nodetype=storage" << std::endl;
}

void ModeListMirrorBuddyGroups::printGroups(NodeType nodeType, UInt16List& buddyGroupIDs,
   UInt16List& primaryTargetIDs, UInt16List& secondaryTargetIDs)
{
   if(buddyGroupIDs.empty() )
   {
      std::cerr << "No mirror buddy groups defined." << std::endl;
      return;
   }

   // print header
   if (nodeType == NODETYPE_Meta)
   {
      printf("%17s %17s %17s\n", "BuddyGroupID", "PrimaryNodeID", "SecondaryNodeID");
      printf("%17s %17s %17s\n", "============", "=============", "===============");
   }
   else
   {
      printf("%17s %17s %17s\n", "BuddyGroupID", "PrimaryTargetID", "SecondaryTargetID");
      printf("%17s %17s %17s\n", "============", "===============", "=================");
   }

   // print buddy group mappings

   for (ZipConstIterRange<UInt16List, UInt16List, UInt16List>
        iter(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs);
        !iter.empty(); ++iter)
      printf("%17hu %17hu %17hu\n", *iter()->first, *iter()->second, *iter()->third);
}

