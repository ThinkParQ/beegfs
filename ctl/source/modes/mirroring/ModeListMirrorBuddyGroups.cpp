#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/ZipIterator.h>

#include <program/Program.h>
#include <modes/Common.h>
#include "ModeListMirrorBuddyGroups.h"

int ModeListMirrorBuddyGroups::execute()
{

   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   ModeHelper::printEnterpriseFeatureMsg();

   NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg);

   if (nodeType != NODETYPE_Meta && nodeType != NODETYPE_Storage)
   {
      std::cerr << "Invalid or missing node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   return ctl::common::downloadMirrorBuddyGroups(nodeType).reduce(
      [=] (const auto& data) {
         printGroups(nodeType, std::get<0>(data), std::get<1>(data), std::get<2>(data));
         return APPCODE_NO_ERROR;
      },
      [] (const auto&) {
         std::cerr << "Download of mirror buddy groups failed." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   );

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

void ModeListMirrorBuddyGroups::printGroups(const NodeType nodeType, const UInt16List& buddyGroupIDs,
   const UInt16List& primaryTargetIDs, const UInt16List& secondaryTargetIDs)
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

