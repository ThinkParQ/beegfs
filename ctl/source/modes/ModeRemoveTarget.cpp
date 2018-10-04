#include <app/App.h>
#include <common/net/message/nodes/UnmapTargetMsg.h>
#include <common/net/message/nodes/UnmapTargetRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/toolkit/ZipIterator.h>
#include <modes/modehelpers/ModeHelperGetNodes.h>
#include <program/Program.h>
#include "ModeRemoveTarget.h"


#define MODEREMOVETARGET_ARG_FORCE  "--force"



int ModeRemoveTarget::execute()
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   uint16_t cfgTargetID;

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments   

   if(cfg->empty() )
   {
      std::cerr << "No target ID specified." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   std::string targetIDStr = cfg->begin()->first;
   cfgTargetID = StringTk::strToUInt(targetIDStr);
   cfg->erase(cfg->begin() );

   bool isNumericRes = StringTk::isNumeric(targetIDStr);
   if(!isNumericRes)
   {
      std::cerr << "Invalid targetID given (must be numeric): " << targetIDStr << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   if (buddyGroupMapper->getBuddyGroupID(cfgTargetID) != 0)
   {
      std::cerr << "Given target is part of a buddy mirror group. Aborting." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   return removeTarget(cfgTargetID);
}

void ModeRemoveTarget::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <targetID>             The ID of the target that should be removed." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode removes the mapping of a certain target to the corresponding storage" << std::endl;
   std::cout << " server." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Remove target with ID \"5\"" << std::endl;
   std::cout << "  # beegfs-ctl --removetarget 5" << std::endl;
}

/**
 * @return APPCODE_...
 */
int ModeRemoveTarget::removeTarget(uint16_t targetID)
{
   int retVal = APPCODE_RUNTIME_ERROR;

   FhgfsOpsErr removeRes = removeTargetComm(targetID);

   if(removeRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Failed to remove target: " << targetID <<
         " (Error: " << removeRes << ")" << std::endl;
   }
   else
   {
      std::cout << "Removal of target succeeded: " << targetID << std::endl;
      retVal = APPCODE_NO_ERROR;
   }

   return retVal;
}

FhgfsOpsErr ModeRemoveTarget::removeTargetComm(uint16_t targetID)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   auto mgmtNode = mgmtNodes->referenceFirstNode();

   UnmapTargetRespMsg* respMsgCast;

   UnmapTargetMsg msg(targetID);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg, NETMSGTYPE_UnmapTargetResp);
   if (!respMsg)
   {
      //std::cerr << "Network error." << std::endl;
      retVal = FhgfsOpsErr_COMMUNICATION;
      goto err_cleanup;
   }

   respMsgCast = (UnmapTargetRespMsg*)respMsg.get();

   retVal = (FhgfsOpsErr)respMsgCast->getValue();

err_cleanup:
   return retVal;
}

/**
 * Checks if a target is part of a MirrorBuddyGroup
 *
 * @param mgmtNode the management node, must be referenced before given to this method
 * @param targetID the targetID to check
 * @param outIsPartOfMBG out value which is true if the target is part of a MBG, else false
 * @return 0 on success, all other values reports an error (FhgfsOpsErr_...)
 */
FhgfsOpsErr ModeRemoveTarget::isPartOfMirrorBuddyGroup(Node& mgmtNode, uint16_t targetID,
   bool* outIsPartOfMBG)
{
   *outIsPartOfMBG = false;

   // download MirrorBuddyGroups

   UInt16List buddyGroupIDs;
   UInt16List buddyGroupPrimaryTargetIDs;
   UInt16List buddyGroupSecondaryTargetIDs;

   if(!NodesTk::downloadMirrorBuddyGroups(mgmtNode, NODETYPE_Storage, &buddyGroupIDs,
      &buddyGroupPrimaryTargetIDs, &buddyGroupSecondaryTargetIDs, false) )
   {
      std::cerr << "Download of mirror buddy groups failed." << std::endl;
      return FhgfsOpsErr_COMMUNICATION;
   }


   for (ZipIterRange<UInt16List, UInt16List, UInt16List>
        buddyGroupsIter(buddyGroupIDs, buddyGroupPrimaryTargetIDs, buddyGroupSecondaryTargetIDs);
        !buddyGroupsIter.empty(); ++buddyGroupsIter)
   {
      if( (*(buddyGroupsIter()->second) == targetID) || (*(buddyGroupsIter()->third)== targetID) )
      {
         std::cerr << "Removing target " << targetID << " will damage the mirror buddy group " <<
            *(buddyGroupsIter()->first) << ". "
            "Restart the remove command with the parameter --force to ignore this warning."
            << std::endl;

         *outIsPartOfMBG = true;
         break;
      }
   }

   return FhgfsOpsErr_SUCCESS;
}
