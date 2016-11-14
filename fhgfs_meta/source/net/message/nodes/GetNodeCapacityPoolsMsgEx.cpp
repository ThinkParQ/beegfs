#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/nodes/TargetCapacityPools.h>
#include <program/Program.h>
#include "GetNodeCapacityPoolsMsgEx.h"

bool GetNodeCapacityPoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetNodeCapacityPools incoming";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a GetNodeCapacityPoolsMsg from: " + ctx.peerName() );

   CapacityPoolQueryType poolType = getCapacityPoolQueryType();

   LOG_DEBUG(logContext, Log_SPAM, "PoolType: " + StringTk::intToStr(poolType) );

   App* app = Program::getApp();

   UInt16List listNormal;
   UInt16List listLow;
   UInt16List listEmergency;

   switch(poolType)
   {
      case CapacityPoolQuery_META:
      {
         NodeCapacityPools* pools = app->getMetaCapacityPools();
         pools->getPoolsAsLists(listNormal, listLow, listEmergency);
      } break;

      case CapacityPoolQuery_STORAGE:
      {
         TargetCapacityPools* pools = app->getStorageCapacityPools();
         pools->getPoolsAsLists(listNormal, listLow, listEmergency);
      } break;

      case CapacityPoolQuery_STORAGEBUDDIES:
      {
         NodeCapacityPools* pools = app->getStorageBuddyCapacityPools();
         pools->getPoolsAsLists(listNormal, listLow, listEmergency);
      } break;

      default:
      {
         LogContext(logContext).logErr("Invalid pool type: " + StringTk::intToStr(poolType) );

         return false;
      } break;
   }

   ctx.sendResponse(GetNodeCapacityPoolsRespMsg(&listNormal, &listLow, &listEmergency) );

   return true;
}

