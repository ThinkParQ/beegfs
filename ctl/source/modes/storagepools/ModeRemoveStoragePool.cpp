#include "ModeRemoveStoragePool.h"

#include <app/App.h>
#include <common/net/message/nodes/storagepools/RemoveStoragePoolMsg.h>
#include <common/net/message/nodes/storagepools/RemoveStoragePoolRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

int ModeRemoveStoragePool::execute()
{
   ModeHelper::printEnterpriseFeatureMsg();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   StringMap* cfg = Program::getApp()->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   if (cfg->empty() )
   {
      std::cerr << "No storage pool ID specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   bool isNumericRes = StringTk::isNumeric(cfg->begin()->first);
   if(!isNumericRes)
   {
      std::cerr << "Invalid storage pool ID given (must be numeric): "
                << cfg->begin()->first << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   StoragePoolId id;
   id.fromStr(cfg->begin()->first);
   cfg->erase(cfg->begin() );

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   NodeStoreServers* mgmtNodes = Program::getApp()->getMgmtNodes();
   NodeHandle mgmtNode = mgmtNodes->referenceFirstNode();

   if (!mgmtNode)
   {
      std::cerr << "Management node not defined." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   std::cout
         << "Removing storage pool " << id
         << " will move all targets of the pool to the default pool. " << std::endl
         << "Furthermore, ID " << id << " will be reused for new pools, "
         << "i.e. if the pool ID is still referenced in stripe patterns, "
         << "the stripe patterns will automatically reference any new pool with ID " << id << "."
         << std::endl << std::endl;

   std::string input;
   while (input.size() != 1 || !strchr("YyNn", input[0]))
   {
      std::cout << "Do you really want to continue? (y/n)" << std::endl;
      std::getline(std::cin, input);
   }

    switch (input[0])
    {
       case 'Y':
       case 'y':
          break; // just do nothing and go ahead
       case 'N':
       case 'n':
       default:
          // abort here
          return APPCODE_NO_ERROR;
    }

   RemoveStoragePoolRespMsg* respMsgCast;

   RemoveStoragePoolMsg msg(id);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
         NETMSGTYPE_RemoveStoragePoolResp);

   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << mgmtNode->getNodeIDWithTypeStr()
         << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   respMsgCast = static_cast<RemoveStoragePoolRespMsg*>(respMsg.get());

   FhgfsOpsErr rmRes = respMsgCast->getResult();

   if (rmRes == FhgfsOpsErr_SUCCESS)
   {
      std::cout << "Successfully removed storage pool with ID " << id << "." << std::endl;
   }
   else if (rmRes == FhgfsOpsErr_UNKNOWNPOOL)
   {
      std::cerr << "Failed to remove storage pool, because pool with ID "
                << id << " doesn't exist." << std::endl;
   }
   else if (rmRes == FhgfsOpsErr_INVAL)
   {
      std::cerr << "The management daemon refused to delete storage pool with ID " << id << ". "
                << "The storage pool is probably the default pool." << std::endl;
   }
   else
   {
      std::cerr << "Failed to remove storage pool with ID " << id
                << ". Please see the management daemon log file for more information." << std::endl;
   }

   return APPCODE_NO_ERROR;
}

void ModeRemoveStoragePool::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <poolID>   The ID of the storage pool to remove." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode removes a storage pool from the system." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: This mode removes a storage pool with the ID \"3\"." << std::endl;
   std::cout << "  # beegfs-ctl --removestoragepool 3" << std::endl;
}
