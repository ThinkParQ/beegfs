#include <app/App.h>
#include <common/net/message/nodes/MapTargetsMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <modes/modehelpers/ModeHelperGetNodes.h>
#include <program/Program.h>
#include "ModeMapTarget.h"


#define MODEMAPTARGET_ARG_TARGETID      "--targetid"
#define MODEMAPTARGET_ARG_NODEID        "--nodeid"
#define MODEMAPTARGET_ARG_STORAGEPOOL   "--storagepoolid"

int ModeMapTarget::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   uint16_t cfgTargetID;
   uint16_t cfgNodeID;
   StoragePoolId cfgStoragePoolId;

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments

   StringMapIter iter = cfg->find(MODEMAPTARGET_ARG_TARGETID);
   if(iter != cfg->end() )
   { // found a targetID
      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid targetID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgTargetID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }
   else
   { // targetID is mandatory but wasn't found
      std::cerr << "No target ID specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   iter = cfg->find(MODEMAPTARGET_ARG_NODEID);
   if(iter != cfg->end() )
   { // found a nodeID
      cfgNodeID = StringTk::strToUInt(iter->second);

      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid nodeID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfg->erase(iter);
   }
   else
   { // nodeID is mandatory but wasn't found
      std::cerr << "No node ID specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   iter = cfg->find(MODEMAPTARGET_ARG_STORAGEPOOL);
   if(iter != cfg->end() )
   { // found a storage pool ID

      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid storagePoolID given (must be numeric): " << iter->second
                   << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgStoragePoolId.fromStr(iter->second);

      cfg->erase(iter);
   }
   else
   {
      cfgStoragePoolId = StoragePoolStore::DEFAULT_POOL_ID;
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   retVal = mapTarget(cfgTargetID, cfgNodeID, cfgStoragePoolId);

   return retVal;
}

void ModeMapTarget::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --targetid=<targetID>  The ID of the target that should be mapped to a node." << std::endl;
   std::cout << "  --nodeid=<nodeID>      The ID of the storage server with access to the target." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --storagepoolid=<poolID> The ID of a storage pool for the newly mapped target" << std::endl;
   std::cout << "                           (defaults to the default pool ID)." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode maps a storage target to a certain storage server." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Map target with ID \"10\" to storage server with ID \"5\"" << std::endl;
   std::cout << "  # beegfs-ctl --maptarget --targetid=10 --nodeid=5" << std::endl;
}

/**
 * @return APPCODE_...
 */
int ModeMapTarget::mapTarget(uint16_t targetID, uint16_t nodeID, StoragePoolId storagePoolId)
{
   int retVal = APPCODE_RUNTIME_ERROR;

   FhgfsOpsErr mapRes = mapTargetComm(targetID, nodeID, storagePoolId);

   if(mapRes != FhgfsOpsErr_SUCCESS)
   {
      std::cout << "Mapping failed: " <<
         "target " << targetID << " -> " << "storage " << nodeID << " " <<
         "(Error: " << mapRes << ")" << std::endl;
   }
   else
   {
      std::cout << "Mapping succeeded: " <<
         "target " << targetID << " -> " << "storage " << nodeID << std::endl;
      retVal = APPCODE_NO_ERROR;
   }

   return retVal;
}

FhgfsOpsErr ModeMapTarget::mapTargetComm(uint16_t targetID, uint16_t nodeID,
      StoragePoolId storagePoolId)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   auto mgmtNode = mgmtNodes->referenceFirstNode();

   MapTargetsRespMsg* respMsgCast;

   std::map<uint16_t, StoragePoolId> targets = {{targetID, storagePoolId}};
   MapTargetsMsg msg(targets, NumNodeID(nodeID));

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg, NETMSGTYPE_MapTargetsResp);
   if (!respMsg)
   {
      //std::cerr << "Network error." << std::endl;
      retVal = FhgfsOpsErr_COMMUNICATION;
      goto err_cleanup;
   }

   respMsgCast = (MapTargetsRespMsg*)respMsg.get();

   // we only send one target, so we only get one response
   retVal = respMsgCast->getResults().at(targetID);

err_cleanup:
   return retVal;
}

