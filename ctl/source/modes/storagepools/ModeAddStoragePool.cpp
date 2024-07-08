#include "ModeAddStoragePool.h"
#include "modes/modehelpers/ModeHelper.h"

#include <app/App.h>
#include <common/net/message/nodes/storagepools/AddStoragePoolMsg.h>
#include <common/net/message/nodes/storagepools/AddStoragePoolRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#define MODEADDSTORAGEPOOL_ARG_POOLID       "--id"
#define MODEADDSTORAGEPOOL_ARG_DESCRIPTION  "--desc"
#define MODEADDSTORAGEPOOL_ARG_TARGETS      "--targets"
#define MODEADDSTORAGEPOOL_ARG_MIRRORGROUPS "--mirrorgroups"

int ModeAddStoragePool::execute()
{
   ModeHelper::printEnterpriseFeatureMsg();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   StringMap* cfg = Program::getApp()->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   // check if poolID is set
   StoragePoolId poolId = StoragePoolStore::INVALID_POOL_ID;
   iter = cfg->find(MODEADDSTORAGEPOOL_ARG_POOLID);
   if (iter != cfg->end() )
   {
      poolId.fromStr(iter->second);
      cfg->erase(iter);
   }
   
   // check if description is set
   iter = cfg->find(MODEADDSTORAGEPOOL_ARG_DESCRIPTION);
   if (iter == cfg->end() )
   {
      std::cerr << MODEADDSTORAGEPOOL_ARG_DESCRIPTION << " must be set." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   // set description
   std::string description = iter->second;
   cfg->erase(iter);

   // check if target list is set
   std::string targetsStr;
   UInt16Set targets;

   iter = cfg->find(MODEADDSTORAGEPOOL_ARG_TARGETS);
   if (iter != cfg->end())
   {
      targetsStr = iter->second;
      StringVector targetsStrVec;

      StringTk::explode(targetsStr, ',', &targetsStrVec);

      for (auto it = targetsStrVec.begin(); it != targetsStrVec.end(); it++)
      {
         if (StringTk::isNumeric(*it))
         {
            targets.insert(StringTk::strToUInt(*it));
         }
         else
         {
            std::cerr << MODEADDSTORAGEPOOL_ARG_TARGETS
               << " must be a comma-separated list of numeric values." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
      }

      cfg->erase(iter);
   }

   // check if buddy groups list is set
   std::string buddyGroupsStr;
   UInt16Set buddyGroups;

   iter = cfg->find(MODEADDSTORAGEPOOL_ARG_MIRRORGROUPS);
   if (iter != cfg->end())
   {
      buddyGroupsStr = iter->second;
      StringVector buddyGroupsStrVec;

      StringTk::explode(buddyGroupsStr, ',', &buddyGroupsStrVec);

      for (auto it = buddyGroupsStrVec.begin(); it != buddyGroupsStrVec.end(); it++)
      {
         if (StringTk::isNumeric(*it))
         {
            buddyGroups.insert(StringTk::strToUInt(*it));
         }
         else
         {
            std::cerr << MODEADDSTORAGEPOOL_ARG_MIRRORGROUPS
               << " must be a comma-separated list of numeric values." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
      }

      cfg->erase(iter);
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   NodeStoreServers* mgmtNodes = Program::getApp()->getMgmtNodes();
   NodeHandle mgmtNode = mgmtNodes->referenceFirstNode();

   if (!mgmtNode)
   {
      std::cerr << "Management node not defined." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   // check if any of the targets is part of a buddy group and refuse to move them if so
   MirrorBuddyGroupMapper* buddyGroupMapper = Program::getApp()->getMirrorBuddyGroupMapper();

   bool invalidTargetFound = false;
   for (auto iter = targets.begin(); iter != targets.end(); iter++)
   {
      uint16_t buddyGroupId = buddyGroupMapper->getBuddyGroupID(*iter);

      if (buddyGroupId != 0)
      {
         invalidTargetFound = true;

         std::cerr << "Refusing to move target with ID " << *iter << ". "
                   << "Target is part of a buddy group. "
                   << "Please use the buddy group instead of the target." << std::endl;
      }
   }

   if (invalidTargetFound)
   {
      std::cerr << std::endl
                << "Note: Due to the above errors, the operation was not performed."
                << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   // send the actual message
   AddStoragePoolRespMsg* respMsgCast;

   AddStoragePoolMsg msg(poolId, description, &targets, &buddyGroups);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg, NETMSGTYPE_AddStoragePoolResp);

   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << mgmtNode->getNodeIDWithTypeStr()
         << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   respMsgCast = static_cast<AddStoragePoolRespMsg*>(respMsg.get());

   FhgfsOpsErr addRes = respMsgCast->getResult();

   switch (addRes)  
   {  
      case FhgfsOpsErr_SUCCESS:  
      {
         std::cout << "Successfully created storage pool."
                   << std::endl << std::endl
                   << "ID: " << respMsgCast->getPoolId() << "; "
                   << "Description: " << description
                   << std::endl;
         
         return APPCODE_NO_ERROR;
      } break;
      
      case FhgfsOpsErr_INVAL:  
      {
         std::cerr << "Successfully created storage pool, "
                   << "but not all requested targets or buddy groups could be moved to new pool."
                   << std::endl << std::endl
                   << "ID: " << respMsgCast->getPoolId() << "; "
                   << "Description: " << description
                   << std::endl;
         
         return APPCODE_INVALID_RESULT;
      } break;
      
      case FhgfsOpsErr_EXISTS:  
      {
         std::cerr << "Couldn't create storage pool. A storage pool with this ID exists already." 
                   << std::endl;
         return APPCODE_INVALID_RESULT;
      } break;
      
      default:  
      {
         std::cerr << "Failed to create storage pool." << std::endl;
         return APPCODE_INVALID_RESULT;
      }
   }  
}

void ModeAddStoragePool::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --desc=<description>   A description to identify the new storage pool." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --id=<poolID>                         A numeric ID for the new pool. Will be" << std::endl; 
   std::cout << "                                        automatically generated if not provided." << std::endl;
   std::cout << "  --targets=<targetID1,targetID2,...>   A comma-separated list of targets to add" << std::endl;
   std::cout << "                                        to the new pool." << std::endl;
   std::cout << "  --mirrorgroups=<groupID1,groupID2,...> A comma-separated list of storage mirror" << std::endl;
   std::cout << "                                        buddy groups to add to the new pool." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode adds a storage target pool." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Generate a pool with the description 'fastPool' and the targets" << std::endl;
   std::cout << "'1', '2', '8' and '9'." << std::endl;
   std::cout << "  # beegfs-ctl --addstoragepool --desc=fastpool --targets=1,2,8,9" << std::endl;
}
