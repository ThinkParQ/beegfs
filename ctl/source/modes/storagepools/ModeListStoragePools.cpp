#include "ModeListStoragePools.h"

#include <app/App.h>
#include <common/net/message/nodes/storagepools/GetStoragePoolsMsg.h>
#include <common/net/message/nodes/storagepools/GetStoragePoolsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>

#include <boost/format.hpp>

int ModeListStoragePools::execute()
{
   ModeHelper::printEnterpriseFeatureMsg();

   // check arguments
   StringMap* cfg = Program::getApp()->getConfig()->getUnknownConfigArgs();

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   NodeStoreServers* mgmtNodes = Program::getApp()->getMgmtNodes();
   NodeHandle mgmtNode = mgmtNodes->referenceFirstNode();

   if (!mgmtNode)
   {
      std::cerr << "Management node not defined." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   GetStoragePoolsRespMsg* respMsgCast;

   GetStoragePoolsMsg msg;

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
         NETMSGTYPE_GetStoragePoolsResp);

   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << mgmtNode->getNodeIDWithTypeStr()
         << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   respMsgCast = static_cast<GetStoragePoolsRespMsg*>(respMsg.get());

   printPools(respMsgCast->getStoragePools());

   return APPCODE_NO_ERROR;
}

void ModeListStoragePools::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode lists all storage target pools." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: List all pools." << std::endl;
   std::cout << "  $ beegfs-ctl --liststoragepools" << std::endl;
}

void ModeListStoragePools::printPools(const StoragePoolPtrVec& pools) const
{
   std::cout << boost::format("%7s %18s %28s %28s\n") % "Pool ID"
                                                      % "Pool Description"
                                                      % "Targets"
                                                      % "Buddy Groups";
   std::cout << boost::format("%7s %18s %28s %28s\n")
      % "======="
      % "=================="
      % "============================"
      % "============================";

   for (StoragePoolPtrVecCIter poolIter = pools.begin(); poolIter != pools.end(); poolIter++)
   {
      const UInt16Set targets = (*poolIter)->getTargets();
      StringVector targetsStrVec = StringTk::implodeMultiLine (targets, ',', 28);

      const UInt16Set buddyGroups = (*poolIter)->getBuddyGroups();
      StringVector buddyGroupsStrVec = StringTk::implodeMultiLine (buddyGroups, ',', 28);

      std::cout << boost::format ("%7i %18s %|-28s| %|-28s|\n") % (*poolIter)->getId()
                % (*poolIter)->getDescription()
                % targetsStrVec[0]
                % buddyGroupsStrVec[0];

      size_t targetsStrVecSize = targetsStrVec.size();
      size_t buddyGroupsStrVecSize = buddyGroupsStrVec.size();

      if (targetsStrVecSize > 1 || buddyGroupsStrVecSize > 1)
      {
         if (targetsStrVecSize > buddyGroupsStrVecSize)
         {
            buddyGroupsStrVec.resize (targetsStrVecSize);
         }
         else if (buddyGroupsStrVecSize > targetsStrVecSize)
         {
            targetsStrVec.resize (buddyGroupsStrVecSize);
         }

         for (size_t i = 1; i < targetsStrVecSize; i++)
         {
            std::cout << boost::format ("%7i %18s %|-28s| %|-28s|\n") 
                      % ""
                      % ""
                      % targetsStrVec[i]
                      % buddyGroupsStrVec[i];
         }
      }
   }
}
