#include <app/App.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/storage/mirroring/SetLastBuddyCommOverrideMsg.h>
#include <common/net/message/storage/mirroring/SetLastBuddyCommOverrideRespMsg.h>
#include <common/nodes/NodeStore.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>

#include "ModeStartResync.h"

#define MODESTARTSTORAGERESYNC_ARG_TARGETID      "--targetid"
#define MODESTARTSTORAGERESYNC_ARG_NODEID        "--nodeid"
#define MODESTARTSTORAGERESYNC_ARG_GROUPID       "--mirrorgroupid"
#define MODESTARTSTORAGERESYNC_ARG_TIMESPAN      "--timespan"
#define MODESTARTSTORAGERESYNC_ARG_TIMESTAMP     "--timestamp"
#define MODESTARTSTORAGERESYNC_ARG_RESTART       "--restart"

int ModeStartResync::execute()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   uint16_t cfgTargetID = 0;
   uint16_t cfgBuddyID = 0;
   int64_t cfgTimestamp = -1;
   bool cfgRestart = false;
   bool isBuddyGroupID = false;

   ModeHelper::printEnterpriseFeatureMsg();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments

   this->nodeType = ModeHelper::nodeTypeFromCfg(cfg);

   if (this->nodeType != NODETYPE_Meta && this->nodeType != NODETYPE_Storage)
   {
      std::cerr << "Invalid or missing node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   StringMapIter iter = cfg->find(MODESTARTSTORAGERESYNC_ARG_TARGETID);
   if(iter != cfg->end() )
   { // found a targetID
      if (nodeType == NODETYPE_Meta)
      {
         std::cerr << "TargetIDs are only supported when resyncing storage targets. "
            "For metadata servers, plase use the --nodeID parameter." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid targetID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgTargetID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODESTARTSTORAGERESYNC_ARG_NODEID);
   if (iter !=cfg->end())
   {
      if (nodeType == NODETYPE_Storage)
      {
         std::cerr << "NodeIDs are only supported when resyncing metadata nodes. "
            "For storage targets, please use the --targetID parameter." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      bool isNumericRes = StringTk::isNumeric(iter->second);
      if (!isNumericRes)
      {
         std::cerr << "Invalid nodeID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgTargetID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODESTARTSTORAGERESYNC_ARG_GROUPID);
   if(iter != cfg->end() )
   { // found a buddyGroupID
      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid buddy group ID given (must be numeric): " << iter->second
            << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgBuddyID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }


   iter = cfg->find(MODESTARTSTORAGERESYNC_ARG_TIMESTAMP);
   if(iter != cfg->end() )
   { // found a timestamp
      if (nodeType == NODETYPE_Meta)
      {
         std::cerr << "Metadata nodes can only do full resync. --timestamp is not supported."
            << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid timestamp given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgTimestamp = StringTk::strToInt64(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODESTARTSTORAGERESYNC_ARG_TIMESPAN);
   if(iter != cfg->end() )
   { // found a timespan

      if (nodeType == NODETYPE_Meta)
      {
         std::cerr << "Metadata nodes can only do full resync. --timespan is not supported."
            << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if (cfgTimestamp >= 0)
      {
         std::cerr << "--timestamp and --timespan cannot be used at the same time." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      bool isValid = UnitTk::isValidHumanTimeString(iter->second);
      if(!isValid)
      {
         std::cerr << "Invalid timespan given (must be numeric with a unit of sec/min/h/d "
            "(seconds/minutes/hours/days): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      // valid timespan => convert to secondes and substract from current timestamp
      int64_t timespanSecs = UnitTk::timeStrHumanToInt64(iter->second);
      int64_t currentTime = time(NULL);

      cfgTimestamp = currentTime-timespanSecs;
      cfg->erase(iter);
   }

   iter = cfg->find(MODESTARTSTORAGERESYNC_ARG_RESTART);
   if(iter != cfg->end() )
   { // restart flag set
      if (nodeType == NODETYPE_Meta)
      {
         std::cerr << "It is not possible to abort and restart a running metadata server resync."
            << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if (cfgTimestamp < 0)
      {
         std::cerr << "Resync can only be restarted in combination with --timestamp or --timespan." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgRestart = true;
      cfg->erase(iter);
   }

   if ( (cfgTargetID != 0) && (cfgBuddyID != 0) )
   {
      std::cerr << "--targetID and --mirrorgroupid cannot be used at the same time." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else
   if ( (cfgTargetID == 0 ) && (cfgBuddyID == 0))
   {
      std::cerr << "Either --targetID or --mirrorgroupid must be given." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else
   if (cfgBuddyID != 0)
   {
      cfgTargetID = cfgBuddyID;
      isBuddyGroupID = true;
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   int resyncRes = requestResync(cfgTargetID, isBuddyGroupID, cfgTimestamp, cfgRestart);

   return resyncRes;
}

void ModeStartResync::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --nodetype=<nodetype>           The node type (metadata, storage)." << std::endl;
   std::cout << " One of:" << std::endl;
   std::cout << "  --targetid=<targetID>           The ID of the target that should be re-synced." << std::endl;
   std::cout << "                                  (only for nodetype=storage)" << std::endl;
   std::cout << "  --nodeid=<nodeID>               The ID of the node that should be re-synced." << std::endl;
   std::cout << "                                  (only for nodetype=metadata)" << std::endl;
   std::cout << "  --mirrorgroupid=<mirrorGroupID> Resync the secondary target of this" << std::endl;
   std::cout << "                                  mirror buddy group." << std::endl;
   std::cout << std::endl;
   std::cout << " Note: --mirrorgroupid can not be used together with --targetid or --nodeid." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional (only for nodetype=storage):" << std::endl;
   std::cout << "  --timestamp=<unixTimestamp>     Override last buddy communication timestamp." << std::endl;
   std::cout << "  --timespan=<timespan>           Resync entries modified in the given timespan." << std::endl;
   std::cout << "  --restart                       Abort any running resyncs and start a new one." << std::endl;
   std::cout << std::endl;
   std::cout << " Note: --restart can only be used in combination with --timestamp or --timespan." << std::endl;
   std::cout << " Note: Either --timestamp or --timespan can be used, not both." << std::endl;
   std::cout << " Note: --timespan must have exactly one unit. Possible units are (s)econds, " << std::endl;
   std::cout << "       (m)inutes, (h)ours and (d)ays." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode starts a resync of a storage target or metadata node from its buddy." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Resync target with ID \"10\" from its buddy using the resync timestamp " << std::endl;
   std::cout << "          saved on the server" << std::endl;
   std::cout << "  # beegfs-ctl --startresync --nodetype=storage --targetid=10" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Resync the changes in buddy group with ID 2 of the last 5 days" << std::endl;
   std::cout << "  # beegfs-ctl --startresync --nodetype=storage --mirrorgroupid=2 --timespan=5d" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Resync the changes in buddy group with ID 2 of the last 36 hours" << std::endl;
   std::cout << "  # beegfs-ctl --startresync --nodetype=storage --mirrorgroupid=2 --timespan=36h" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Perform a full resync of target with ID 5" << std::endl;
   std::cout << "  # beegfs-ctl --startresync --nodetype=storage --targetid=5 --timestamp=0" << std::endl;
}

/**
 * @return APPCODE_...
 */
int ModeStartResync::requestResync(uint16_t syncToTargetID, bool isBuddyGroupID,
   int64_t timestamp, bool restartResync)
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   auto mgmtNode = mgmtNodes->referenceFirstNode();
   TargetMapper* targetMapper = app->getTargetMapper();

   MirrorBuddyGroupMapper* buddyGroupMapper;
   if(this->nodeType == NODETYPE_Storage)
      buddyGroupMapper = app->getMirrorBuddyGroupMapper();
   else
      buddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   NodeList nodeList;
   UInt16List targetIDs;
   UInt16List nodeIDs;
   UInt16List buddyGroupIDs;
   UInt16List buddyPrimaryTargetIDs;
   UInt16List buddySecondaryTargetIDs;
   NumNodeID syncToNodeID;
   uint16_t syncFromTargetID = 0;
   NumNodeID syncFromNodeID;

   FhgfsOpsErr setResyncStateRes;
   FhgfsOpsErr overrideLastBudyCommRes;

   buddyGroupMapper->getMappingAsLists(buddyGroupIDs, buddyPrimaryTargetIDs,
      buddySecondaryTargetIDs);

   // find the target/node IDs to the given buddy group
   if (isBuddyGroupID)
   {
      syncFromTargetID = buddyGroupMapper->getPrimaryTargetID(syncToTargetID);
      syncToTargetID = buddyGroupMapper->getSecondaryTargetID(syncToTargetID);

      if (syncToTargetID == 0)
      {
         std::cerr << "Couldn't find "
            << (nodeType == NODETYPE_Meta ? "nodes" : "targets")
            << " for buddy group ID: " << syncToTargetID << "." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }
   else
   {
      bool isPrimary;
      uint16_t buddyTargetID = buddyGroupMapper->getBuddyTargetID(syncToTargetID, &isPrimary);

      if (buddyTargetID == 0)
      {
         std::cerr << "Couldn't find mirror buddy for "
            << (nodeType == NODETYPE_Meta ? "node" : "target")
            << " ID: " << syncToTargetID << "." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      // swap IDs if we were given the ID of the primary
      if (isPrimary)
      {
         syncFromTargetID = syncToTargetID;
         syncToTargetID = buddyTargetID;
      }
      else
      {
         syncFromTargetID = buddyTargetID;
      }
   }

   if(syncFromTargetID == 0)
   {
      if (this->nodeType == NODETYPE_Storage)
         std::cerr << "Couldn't find primary target. secondary targetID: ";
      else
         std::cerr << "Couldn't find primary node. secondary nodeID: ";

      std::cerr << syncToTargetID << "." << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   // check if a resync is already running (but only if restartResync is not set)
   // if it is set we actually don't care if a resync is still running
   // Note: restartResync is always false when nodeType == NODETYPE_Meta}
   if (!restartResync)
   {
      BuddyResyncJobState resyncJobState;
      FhgfsOpsErr getStatsRes;

      if (nodeType == NODETYPE_Storage)
      {
         StorageBuddyResyncJobStatistics resyncJobStats;
         getStatsRes = ModeHelper::getStorageBuddyResyncStats(syncFromTargetID, resyncJobStats);
         resyncJobState = resyncJobStats.getState();
      }
      else
      {
         MetaBuddyResyncJobStatistics resyncJobStats;
         getStatsRes = ModeHelper::getMetaBuddyResyncStats(syncFromTargetID, resyncJobStats);
         resyncJobState = resyncJobStats.getState();
      }

      if(getStatsRes != FhgfsOpsErr_SUCCESS)
      {
         if (this->nodeType == NODETYPE_Storage)
            std::cerr << "Could not check for running jobs. targetID: ";
         else
            std::cerr << "Could not check for running jobs. nodeID: ";

         std::cerr << syncFromTargetID << "; error: " << getStatsRes << std::endl;

         return APPCODE_RUNTIME_ERROR;
      }
      else if(resyncJobState == BuddyResyncJobState_RUNNING)
      {
         if (this->nodeType == NODETYPE_Storage)
            std::cerr << "Resync already running. targetID: "
               << syncFromTargetID <<
               "; Use option --restart to abort running resync and start over." << std::endl;
         else
            std::cerr << "Resync already running. nodeID: " << syncFromTargetID << "." << std::endl;

         return APPCODE_NO_ERROR;
      }
   }

   if (this->nodeType == NODETYPE_Storage)
   {
      // get the storage nodes corresponding to the IDs
      syncToNodeID = targetMapper->getNodeID(syncToTargetID);
      syncFromNodeID = targetMapper->getNodeID(syncFromTargetID);
   }
   else
   {
      // The IDs we got from the buddy group mapper are nodeIDs.
      syncToNodeID = NumNodeID(syncToTargetID);
      syncFromNodeID = NumNodeID(syncFromTargetID);
   }

   // Note: Timestamp is only supported by storage nodes - the execute function already made sure
   // that we only have a timestamp when we're talking to storage nodes.
   if(timestamp >= 0)
   {
      overrideLastBudyCommRes = overrideLastBuddyComm(syncFromTargetID, syncFromNodeID, timestamp,
         restartResync);

      if(overrideLastBudyCommRes != FhgfsOpsErr_SUCCESS)
      {
         std::cerr << "Could not override last buddy communication timestamp.";

         std::cerr << "Resync request not successful; primary target ID: "
            << syncFromTargetID << "; nodeID: "
            << std::endl;

         return APPCODE_RUNTIME_ERROR;
      }
   }

   // Note: restartResync is is always false when nodeType == NODETYPE_Meta.
   if (restartResync)
   {
      std::cout << "Waiting for running resyncs to abort. " << std::endl;
      TargetConsistencyState secondaryState;

      // to avoid races we have to wait for the node to leave needs_resync state; this usually
      // happens because resync will set secondary to state BAD if it gets aborted.
      do
      {
         bool downloadStatesRes;
         UInt16List targetIDs;
         UInt8List reachabilityStates;
         UInt8List consistencyStates;

         downloadStatesRes = NodesTk::downloadTargetStates(*mgmtNode, this->nodeType, &targetIDs,
            &reachabilityStates, &consistencyStates, false);

         if(!downloadStatesRes)
         {
            std::cerr << "Download of target states failed. " << std::endl;
            return APPCODE_RUNTIME_ERROR;
         }

         bool found = false;
         for (ZipIterRange<UInt16List, UInt8List> iter(targetIDs, consistencyStates);
              !iter.empty(); ++iter)
         {
            if(*iter()->first == syncToTargetID)
            {
               secondaryState = (TargetConsistencyState)*(iter()->second);
               found = true;
               break;
            }
         }

         if (!found)
         {
            std::cerr << "Download of target states failed; target ID unknown. " << std::endl;
            return APPCODE_RUNTIME_ERROR;
         }

         PThread::sleepMS(2000);
      } while (secondaryState == TargetConsistencyState_NEEDS_RESYNC);
   }

   setResyncStateRes = setResyncState(syncToTargetID, syncToNodeID);

   if(setResyncStateRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Resync request not successful; " <<
         (isBuddyGroupID ? "buddy group ID: " : "secondary target ID: ") << syncToTargetID
         << "; nodeID: " << syncToNodeID << "; Error: " << setResyncStateRes << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }
   else
   {
      std::cout << "Resync request sent. " << std::endl;
      return APPCODE_NO_ERROR;
   }
}

FhgfsOpsErr ModeStartResync::setResyncState(uint16_t targetID, NumNodeID nodeID)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();

   NodeStore* nodes;
   NodeHandle node;

   if (this->nodeType == NODETYPE_Storage)
   {
      nodes = app->getStorageNodes();
      node = nodes->referenceNode(nodeID);
   }
   else
   {
      nodes = app->getMetaNodes();
      node = nodes->referenceNode(nodeID);
   }

   if (!node)
      return FhgfsOpsErr_UNKNOWNNODE;

   UInt16List targetIDs(1, targetID);
   UInt8List states(1, (uint8_t)TargetConsistencyState_NEEDS_RESYNC);

   SetTargetConsistencyStatesRespMsg* respMsgCast;

   SetTargetConsistencyStatesMsg msg(nodeType, &targetIDs, &states, false);

   const auto respMsg = MessagingTk::requestResponse(*node, msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp);
   if (!respMsg)
   {
      retVal = FhgfsOpsErr_COMMUNICATION;
      goto err_cleanup;
   }

   respMsgCast = (SetTargetConsistencyStatesRespMsg*)respMsg.get();

   retVal = respMsgCast->getResult();

err_cleanup:
   return retVal;
}

FhgfsOpsErr ModeStartResync::overrideLastBuddyComm(uint16_t targetID, NumNodeID nodeID,
   int64_t timestamp, bool restartResync)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   NodeStore* nodes;
   NodeHandle node;

   if (this->nodeType == NODETYPE_Storage)
   {
      nodes = app->getStorageNodes();
      node = nodes->referenceNode(nodeID);
   }
   else
   {
      nodes = app->getMetaNodes();
      node = nodes->referenceNode(nodeID);
   }

   if (!node)
   {
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   SetLastBuddyCommOverrideRespMsg* respMsgCast;

   SetLastBuddyCommOverrideMsg msg(targetID, timestamp, restartResync);

   const auto respMsg = MessagingTk::requestResponse(*node, msg,
         NETMSGTYPE_SetLastBuddyCommOverrideResp);
   if (!respMsg)
   {
      retVal = FhgfsOpsErr_COMMUNICATION;
      goto err_cleanup;
   }

   respMsgCast = (SetLastBuddyCommOverrideRespMsg*)respMsg.get();

   retVal = (FhgfsOpsErr)respMsgCast->getValue();

err_cleanup:
   return retVal;
}
