#include "ModifyStoragePoolMsgEx.h"

#include <common/app/log/LogContext.h>
#include <common/net/message/nodes/storagepools/ModifyStoragePoolRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/nodes/StoragePoolStore.h>
#include <program/Program.h>

bool ModifyStoragePoolMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(STORAGEPOOLS, DEBUG, "Received a ModifyStoragePoolMsg", ctx.peerName());

   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   StoragePoolStore* storagePoolStore = Program::getApp()->getStoragePoolStore();

   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   FhgfsOpsErr result = FhgfsOpsErr_SUCCESS;
   bool changesMade = false;

   // check if pool exists
   bool poolExists = storagePoolStore->poolExists(poolId);

   if (!poolExists)
   {
      LOG(STORAGEPOOLS, WARNING, "Storage pool ID doesn't exist", poolId);

      result = FhgfsOpsErr_INVAL;

      goto send_response;
   }


   bool setDescriptionResp;
   if (newDescription && !newDescription->empty()) // changeName
   {
      setDescriptionResp = storagePoolStore->setPoolDescription(poolId, *newDescription);

      if (setDescriptionResp == FhgfsOpsErr_SUCCESS)
      {
         changesMade = true;
      }
      else
      {
         LOG(STORAGEPOOLS, WARNING, "Could not set new description for storage pool");

         result = FhgfsOpsErr_INTERNAL;
      }
   }
   else
   {
      setDescriptionResp = false; // needed later for notifications to metadata nodes
   }

   if (addTargets) // add targets to the pool
   {               // -> this can only happen if targets are in the default pool
      for (auto it = addTargets->begin(); it != addTargets->end(); it++)
      {
         FhgfsOpsErr moveTargetsResp =
            storagePoolStore->moveTarget(StoragePoolStore::DEFAULT_POOL_ID, poolId, *it);

         if (moveTargetsResp == FhgfsOpsErr_SUCCESS)
         {
            changesMade = true;
         }
         else
         {
            LOG(STORAGEPOOLS, WARNING,
               "Could not add target to storage pool. "
               "Probably the target doesn't exist or is not a member of the default storage pool.",
               poolId, as("targetId",*it));

            result = FhgfsOpsErr_INTERNAL;

            // however, we still try to move the remaining targets
         }
      }
   }

   if (rmTargets)  // remove targets from the pool (i.e. move them to the default pool)
   {
      for (auto it = rmTargets->begin(); it != rmTargets->end(); it++)
      {
         FhgfsOpsErr moveTargetsResp =
            storagePoolStore->moveTarget(poolId, StoragePoolStore::DEFAULT_POOL_ID, *it);


         if (moveTargetsResp == FhgfsOpsErr_SUCCESS)
         {
            changesMade = true;
         }
         else
         {
            LOG(STORAGEPOOLS, WARNING,
               "Could not remove target from storage pool. "
               "Probably the target doesn't exist or is not a member of the storage pool.",
               poolId, as("targetId",*it));

            result = FhgfsOpsErr_INTERNAL;

            // however, we still try to move the remaining targets
         }
      }
   }

   if (addBuddyGroups) // add targets to the pool
   {               // -> this can only happen if targets are in the default pool
      for (auto it = addBuddyGroups->begin(); it != addBuddyGroups->end(); it++)
      {
         FhgfsOpsErr moveBuddyGroupsResp =
            storagePoolStore->moveBuddyGroup(StoragePoolStore::DEFAULT_POOL_ID, poolId, *it);

         if (moveBuddyGroupsResp == FhgfsOpsErr_SUCCESS)
         {
            changesMade = true;
         }
         else
         {
            LOG(STORAGEPOOLS, WARNING,
               "Could not add buddy group to storage pool. "
               "Probably the buddy group doesn't exist "
               "or is not a member of the default storage pool.",
               poolId, as("buddyGroupId",*it));

            result = FhgfsOpsErr_INTERNAL;

            // however, we still try to move the remaining buddy groups
         }
      }
   }

   if (rmBuddyGroups)  // remove targets from the pool (i.e. move them to the default pool)
   {
      for (auto it = rmBuddyGroups->begin(); it != rmBuddyGroups->end(); it++)
      {
         FhgfsOpsErr moveBuddyGroupsResp =
            storagePoolStore->moveBuddyGroup(poolId, StoragePoolStore::DEFAULT_POOL_ID, *it);


         if (moveBuddyGroupsResp == FhgfsOpsErr_SUCCESS)
         {
            changesMade = true;
         }
         else
         {
            LOG(STORAGEPOOLS, WARNING,
               "Could not remove buddy group from storage pool. "
               "Probably the buddy group doesn't exist or is not a member of the storage pool.",
               poolId, as("buddy group",*it));

            result = FhgfsOpsErr_INTERNAL;

            // however, we still try to move the remaining buddy groups
         }
      }
   }

   if (changesMade)
   {  // changes were made
      Program::getApp()->getHeartbeatMgr()->notifyAsyncRefreshStoragePools();
   }

send_response:

   ctx.sendResponse(ModifyStoragePoolRespMsg(result));

   return true;
}

