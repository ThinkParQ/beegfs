#include "ModeModifyStoragePool.h"

#include <app/App.h>
#include <common/net/message/nodes/storagepools/ModifyStoragePoolMsg.h>
#include <common/net/message/nodes/storagepools/ModifyStoragePoolRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#define MODEMODIFYSTORAGEPOOL_ARG_POOLID          "--id"
#define MODEMODIFYSTORAGEPOOL_ARG_DESCRIPTION     "--desc"
#define MODEMODIFYSTORAGEPOOL_ARG_ADDTARGETS      "--addtargets"
#define MODEMODIFYSTORAGEPOOL_ARG_RMTARGETS       "--rmtargets"
#define MODEMODIFYSTORAGEPOOL_ARG_ADDMIRRORGROUPS "--addmirrorgroups"
#define MODEMODIFYSTORAGEPOOL_ARG_RMMIRRORGROUPS  "--rmmirrorgroups"

int ModeModifyStoragePool::execute()
{
   ModeHelper::printEnterpriseFeatureMsg();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   StringMap* cfg = Program::getApp()->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   StoragePoolId poolId;
   std::string description;
   UInt16Vector addTargetsVec;
   UInt16Vector rmTargetsVec;
   UInt16Vector addBuddyGroupsVec;
   UInt16Vector rmBuddyGroupsVec;

   // check if poolId is set
   iter = cfg->find(MODEMODIFYSTORAGEPOOL_ARG_POOLID);

   if (iter == cfg->end()) // poolId not set
   {
      std::cerr << MODEMODIFYSTORAGEPOOL_ARG_POOLID
                << " must be set." << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   if (!StringTk::isNumeric(iter->second))
   {
      std::cerr << MODEMODIFYSTORAGEPOOL_ARG_POOLID
                << " must be a numeric value."
                << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   poolId.fromStr(iter->second);
   cfg->erase(iter);

   // check if description is set
   iter = cfg->find(MODEMODIFYSTORAGEPOOL_ARG_DESCRIPTION);
   if (iter != cfg->end() ) // description set
   {
      description = iter->second;
      cfg->erase(iter);
   }

   // check if addtargets is set
   iter = cfg->find(MODEMODIFYSTORAGEPOOL_ARG_ADDTARGETS);
   if (iter != cfg->end())
   {
      std::string targetsStr = iter->second;
      StringVector targetsStrVec;

      StringTk::explode(targetsStr, ',', &targetsStrVec);

      for (auto it = targetsStrVec.begin(); it != targetsStrVec.end(); it++)
      {
         if (StringTk::isNumeric(*it))
         {
            addTargetsVec.push_back(StringTk::strToUInt(*it));
         }
         else
         {
            std::cerr << MODEMODIFYSTORAGEPOOL_ARG_ADDTARGETS
               << " must be a comma-separated list of numeric values." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
      }

      cfg->erase(iter);
   }

   // check if rmtargets is set
   iter = cfg->find(MODEMODIFYSTORAGEPOOL_ARG_RMTARGETS);
   if (iter != cfg->end())
   {
      std::string targetsStr = iter->second;
      StringVector targetsStrVec;

      StringTk::explode(targetsStr, ',', &targetsStrVec);

      for (auto it = targetsStrVec.begin(); it != targetsStrVec.end(); it++)
      {
         if (StringTk::isNumeric(*it))
         {
            rmTargetsVec.push_back(StringTk::strToUInt(*it));
         }
         else
         {
            std::cerr << MODEMODIFYSTORAGEPOOL_ARG_RMTARGETS
               << " must be a comma-separated list of numeric values." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
      }

      cfg->erase(iter);
   }

   // check if addBuddyGroups is set
   iter = cfg->find(MODEMODIFYSTORAGEPOOL_ARG_ADDMIRRORGROUPS);
   if (iter != cfg->end())
   {
      std::string buddyGroupsStr = iter->second;
      StringVector buddyGroupsStrVec;

      StringTk::explode(buddyGroupsStr, ',', &buddyGroupsStrVec);

      for (auto it = buddyGroupsStrVec.begin(); it != buddyGroupsStrVec.end(); it++)
      {
         if (StringTk::isNumeric(*it))
         {
            addBuddyGroupsVec.push_back(StringTk::strToUInt(*it));
         }
         else
         {
            std::cerr << MODEMODIFYSTORAGEPOOL_ARG_ADDMIRRORGROUPS
               << " must be a comma-separated list of numeric values." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
      }

      cfg->erase(iter);
   }

   // check if rmBuddyGroups is set
   iter = cfg->find(MODEMODIFYSTORAGEPOOL_ARG_RMMIRRORGROUPS);
   if (iter != cfg->end())
   {
      std::string buddyGroupsStr = iter->second;
      StringVector buddyGroupsStrVec;

      StringTk::explode(buddyGroupsStr, ',', &buddyGroupsStrVec);

      for (auto it = buddyGroupsStrVec.begin(); it != buddyGroupsStrVec.end(); it++)
      {
         if (StringTk::isNumeric(*it))
         {
            rmBuddyGroupsVec.push_back(StringTk::strToUInt(*it));
         }
         else
         {
            std::cerr << MODEMODIFYSTORAGEPOOL_ARG_RMMIRRORGROUPS
               << " must be a comma-separated list of numeric values." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
      }

      cfg->erase(iter);
   }

   // check if none or more than one of the exclusive arguments are set
   unsigned numSetArguments = static_cast<unsigned>(!description.empty()) +
                              static_cast<unsigned>(!addTargetsVec.empty()) +
                              static_cast<unsigned>(!rmTargetsVec.empty()) +
                              static_cast<unsigned>(!addBuddyGroupsVec.empty()) +
                              static_cast<unsigned>(!rmBuddyGroupsVec.empty());

   if (numSetArguments != 1)
   {
      std::cerr << "Exactly one of "
                << MODEMODIFYSTORAGEPOOL_ARG_DESCRIPTION
                << "/"
                << MODEMODIFYSTORAGEPOOL_ARG_ADDTARGETS
                << "/"
                << MODEMODIFYSTORAGEPOOL_ARG_RMTARGETS
                << "/"
                << MODEMODIFYSTORAGEPOOL_ARG_ADDMIRRORGROUPS
                << "/"
                << MODEMODIFYSTORAGEPOOL_ARG_RMMIRRORGROUPS
                << " must be set."
                << std::endl;
      return APPCODE_RUNTIME_ERROR;
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
   UInt16Vector buddyMappedTargets;

   for (size_t i = 0; i < addTargetsVec.size(); i++)
   {
      uint16_t buddyGroupId = buddyGroupMapper->getBuddyGroupID(addTargetsVec[i]);

      if (buddyGroupId != 0)
         buddyMappedTargets.push_back(addTargetsVec[i]);
   }

   for (size_t i = 0; i < rmTargetsVec.size(); i++)
   {
      uint16_t buddyGroupId = buddyGroupMapper->getBuddyGroupID(rmTargetsVec[i]);

      if (buddyGroupId != 0)
         buddyMappedTargets.push_back(rmTargetsVec[i]);
   }

   if (!buddyMappedTargets.empty())
   {
      for (size_t i = 0; i < buddyMappedTargets.size(); i++)
      {
         std::cerr << "Refusing to move target with ID " << buddyMappedTargets[i] << ". "
                   << "Target is part of a buddy group. "
                   << "Please move the whole buddy group instead of the target." << std::endl;
      }
      return APPCODE_RUNTIME_ERROR;
   }

   // all fine, send the message
   ModifyStoragePoolRespMsg* respMsgCast;

   ModifyStoragePoolMsg msg(poolId, &addTargetsVec, &rmTargetsVec, &addBuddyGroupsVec,
         &rmBuddyGroupsVec, &description);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
         NETMSGTYPE_ModifyStoragePoolResp);

   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << mgmtNode->getNodeIDWithTypeStr()
         << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   respMsgCast = static_cast<ModifyStoragePoolRespMsg*>(respMsg.get());

   FhgfsOpsErr modifyRes = respMsgCast->getResult();

   if (modifyRes == FhgfsOpsErr_SUCCESS)
   {
      std::cout << "Successfully modified storage pool."
                << std::endl
                << std::endl;
   }
   else if (modifyRes == FhgfsOpsErr_INVAL)
   {
      std::cerr << "Storage Pool ID is unknown."
                << std::endl
                << std::endl;
   }
   else
   {
      std::cerr << "Failed to modify storage pool. "
                << "However, if you requested to add or remove targets, "
                << "parts of the operation might have been successful."
                << std::endl
                << std::endl;
   }

   return APPCODE_NO_ERROR;
}

void ModeModifyStoragePool::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --id=<id>   The ID of the storage pool." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --addtargets=<targetID1,targetID2,...> A comma-separated list of targets to add" << std::endl;
   std::cout << "                                         to the pool." << std::endl;
   std::cout << std::endl;
   std::cout << "  --rmtargets=<targetID1,targetID2,...>  A comma-separated list of targets to" << std::endl;
   std::cout << "                                         remove from the pool." << std::endl;
   std::cout << std::endl;
   std::cout << "  --addmirrorgroups=<mbgID1,mbgID2,...>  A comma-separated list of mirror groups " << std::endl;
   std::cout << "                                         to add to the pool." << std::endl;
   std::cout << std::endl;
   std::cout << "  --rmirrorgroups=<mbgID1,mbgID2,...>    A comma-separated list of mirror groups" << std::endl;
   std::cout << "                                         to remove from the pool." << std::endl;
   std::cout << std::endl;
   std::cout << "  --desc=<description>                   A new description for the pool" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode sets a new description for a storage target pool and adds or removes " << std::endl;
   std::cout << " targets to/from the pool." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set the description 'fastpool' for the storage target pool with ID '5'." << std::endl;
   std::cout << "  # beegfs-ctl --modifystoragepool --id=5 --desc=fastpool" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Add the targets '1','2','7' and '8' to the storage target pool with the "<< std::endl;
   std::cout << " ID '5'." << std::endl;
   std::cout << "  # beegfs-ctl --modifystoragepool --id=5 --addtargets=1,2,7,8" << std::endl;
}
