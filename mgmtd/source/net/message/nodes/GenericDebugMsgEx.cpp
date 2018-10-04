#include <common/net/message/nodes/GenericDebugRespMsg.h>
#include <common/net/msghelpers/MsgHelperGenericDebug.h>
#include <common/toolkit/MessagingTk.h>
#include <nodes/StoragePoolStoreEx.h>
#include <storage/StoragePoolEx.h>
#include <program/Program.h>
#include "GenericDebugMsgEx.h"


#define GENDBGMSG_OP_VERSION              "version"
#define GENDBGMSG_OP_LISTPOOLS            "listpools"
#define GENDBGMSG_OP_LISTSTORAGEPOOLS     "liststoragepools"

bool GenericDebugMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GenericDebugMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_SPAM, std::string("Command string: ") + getCommandStr() );

   std::string cmdRespStr = processCommand();

   ctx.sendResponse(GenericDebugRespMsg(cmdRespStr.c_str() ) );

   return true;
}

/**
 * @return command response string
 */
std::string GenericDebugMsgEx::processCommand()
{
   App* app = Program::getApp();

   std::string responseStr;
   std::string operation;

   // load command string into a stream to allow us to use getline
   std::istringstream commandStream(getCommandStr() );

   // get operation type from command string
   std::getline(commandStream, operation, ' ');

   if(operation == GENDBGMSG_OP_VERSION)
      responseStr = processOpVersion(commandStream);
   else
   if(operation == GENDBGMSG_OP_VARLOGMESSAGES)
      responseStr = MsgHelperGenericDebug::processOpVarLogMessages(commandStream);
   else
   if(operation == GENDBGMSG_OP_VARLOGKERNLOG)
      responseStr = MsgHelperGenericDebug::processOpVarLogKernLog(commandStream);
   else
   if(operation == GENDBGMSG_OP_FHGFSLOG)
      responseStr = MsgHelperGenericDebug::processOpFhgfsLog(commandStream);
   else
   if(operation == GENDBGMSG_OP_LOADAVG)
      responseStr = MsgHelperGenericDebug::processOpLoadAvg(commandStream);
   else
   if(operation == GENDBGMSG_OP_DROPCACHES)
      responseStr = MsgHelperGenericDebug::processOpDropCaches(commandStream);
   else
   if(operation == GENDBGMSG_OP_GETLOGLEVEL)
      responseStr = MsgHelperGenericDebug::processOpGetLogLevel(commandStream);
   else
   if(operation == GENDBGMSG_OP_SETLOGLEVEL)
      responseStr = MsgHelperGenericDebug::processOpSetLogLevel(commandStream);
   else
   if(operation == GENDBGMSG_OP_NETOUT)
      responseStr = MsgHelperGenericDebug::processOpNetOut(commandStream,
         app->getMgmtNodes(), app->getMetaNodes(), app->getStorageNodes() );
   else
   if(operation == GENDBGMSG_OP_LISTMETASTATES)
      responseStr = MsgHelperGenericDebug::processOpListTargetStates(commandStream,
         app->getMetaStateStore() );
   else
   if(operation == GENDBGMSG_OP_LISTSTORAGESTATES)
      responseStr = MsgHelperGenericDebug::processOpListTargetStates(commandStream,
         app->getTargetStateStore() );
   else
   if(operation == GENDBGMSG_OP_QUOTAEXCEEDED)
      responseStr = processOpQuotaExceeded(commandStream);
   else
   if(operation == GENDBGMSG_OP_USEDQUOTA)
      responseStr = processOpUsedQuota(commandStream);
   else
   if(operation == GENDBGMSG_OP_LISTPOOLS)
      responseStr = processOpListPools(commandStream);
   else if(operation == GENDBGMSG_OP_LISTSTORAGEPOOLS)
      responseStr = MsgHelperGenericDebug::processOpListStoragePools(commandStream,
         app->getStoragePoolStore());
   else
      responseStr = "Unknown/invalid operation";

   return responseStr;
}


std::string GenericDebugMsgEx::processOpVersion(std::istringstream& commandStream)
{
   return BEEGFS_VERSION;
}

std::string GenericDebugMsgEx::processOpQuotaExceeded(std::istringstream& commandStream)
{
   App* app = Program::getApp();

   std::string returnString;

   if(!app->getConfig()->getQuotaEnableEnforcement() )
      return "No quota exceeded IDs on this management daemon because quota enforcement is"
         "disabled.";

   // get parameter from command string
   std::string poolIdStr;
   StoragePoolId poolId;

   std::getline(commandStream, poolIdStr, ' ');
   poolId.fromStr(poolIdStr);

   StoragePoolExPtr pool = app->getStoragePoolStore()->getPool(poolId);

   if (!pool)
      return "Storage pool ID could not be resolved to a pool.";

   returnString = MsgHelperGenericDebug::processOpQuotaExceeded(commandStream,
      pool->getExceededQuotaStore() );

   return returnString;
}

std::string GenericDebugMsgEx::processOpUsedQuota(std::istringstream& commandStream)
{
   App* app = Program::getApp();
   std::string returnString;

   if(!app->getConfig()->getQuotaEnableEnforcement() )
      return "No quota data on this management daemon because quota enforcement is disabled.";

   QuotaManager* quotaManager = app->getQuotaManager();

   QuotaDataType quotaDataType = QuotaDataType_NONE;
   bool forEachTarget = false;

   // get parameter from command string
   std::string inputString;
   while(!commandStream.eof() )
   {
      std::getline(commandStream, inputString, ' ');

      if(inputString == "uid")
         quotaDataType = QuotaDataType_USER;
      else
      if(inputString == "gid")
         quotaDataType = QuotaDataType_GROUP;
      else
      if(inputString == "forEachTarget")
         forEachTarget = true;
   }

   // verify given parameters
   if(quotaDataType == QuotaDataType_NONE)
      return "Invalid or missing quota data type argument.";

   if(quotaDataType == QuotaDataType_USER)
      returnString = quotaManager->usedQuotaUserToString(!forEachTarget);
   else
   if(quotaDataType == QuotaDataType_GROUP)
      returnString = quotaManager->usedQuotaGroupToString(!forEachTarget);

   return returnString;
}


/**
 * List internal state of meta and storage capacity pools.
 */
std::string GenericDebugMsgEx::processOpListPools(std::istringstream& commandStream)
{
   // protocol: no arguments

   const App* app = Program::getApp();
   const NodeCapacityPools* metaPools = app->getMetaCapacityPools();
   const NodeCapacityPools* metaBuddyPools = app->getMetaBuddyCapacityPools();

   std::ostringstream responseStream;

   responseStream << "META POOLS STATE: " << std::endl << metaPools->getStateAsStr() << std::endl;

   responseStream << "META BUDDY POOLS STATE: " << std::endl << metaBuddyPools->getStateAsStr()
                  << std::endl;

   const StoragePoolExPtrVec storagePools = app->getStoragePoolStore()->getPoolsAsVec();

   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      const TargetCapacityPools* capPool = (*iter)->getTargetCapacityPools();

      responseStream << "STORAGE CAPACITY POOLS STATE (STORAGE POOL ID: " << (*iter)->getId()
                     << "): " << std::endl << capPool->getStateAsStr() << std::endl;
   }

   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      const NodeCapacityPools* capPool = (*iter)->getBuddyCapacityPools();

      responseStream << "STORAGE BUDDY CAPACITY POOLS STATE (STORAGE POOL ID: "
                     << (*iter)->getId() << "): " << std::endl << capPool->getStateAsStr()
                     << std::endl;
   }

   return responseStream.str();
}
