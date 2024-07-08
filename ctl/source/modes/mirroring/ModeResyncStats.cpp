#include <app/App.h>
#include <common/nodes/NodeStore.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <program/Program.h>

#include "ModeResyncStats.h"

#define MODESTORAGERESYNCSTATS_ARG_TARGETID      "--targetid"
#define MODESTORAGERESYNCSTATS_ARG_NODEID        "--nodeid"
#define MODESTORAGERESYNCSTATS_ARG_GROUPID       "--mirrorgroupid"

int ModeResyncStats::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   uint16_t cfgTargetID = 0;
   uint16_t cfgNodeID = 0;
   uint16_t cfgBuddyID = 0;

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

   {
      StringMapIter iter = cfg->find(MODESTORAGERESYNCSTATS_ARG_TARGETID);
      if (iter != cfg->end())
      { // found a targetID
         if (nodeType == NODETYPE_Meta)
         {
            std::cerr << "Target ID can not be used when querying metadata nodes."
               "Please use --nodeid instead." << std::endl;
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
   }

   {
      StringMapIter iter = cfg->find(MODESTORAGERESYNCSTATS_ARG_NODEID);
      if (iter != cfg->end())
      {
         if (nodeType == NODETYPE_Storage)
         {
            std::cerr << "Node ID can not be used when querying storage servers. "
               "Please use --targetid insead." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         bool isNumericRes = StringTk::isNumeric(iter->second);
         if (!isNumericRes)
         {
            std::cerr << "Invalid nodeID given (must be numeric): " << iter->second << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         cfgNodeID = StringTk::strToUInt(iter->second);
         cfg->erase(iter);
      }
   }

   {
      StringMapIter iter = cfg->find(MODESTORAGERESYNCSTATS_ARG_GROUPID);
      if (iter != cfg->end())
      { // found a buddyGroupID
         bool isNumericRes = StringTk::isNumeric(iter->second);
         if(!isNumericRes)
         {
            std::cerr << "Invalid mirror buddy group ID given (must be numeric): " << iter->second
               << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         cfgBuddyID = StringTk::strToUInt(iter->second);
         cfg->erase(iter);
      }
   }

   if (nodeType == NODETYPE_Storage)
   {
      if ( (cfgTargetID != 0) && (cfgBuddyID != 0) )
      {
         std::cerr << "--targetid and --mirrorgroupid cannot be used at the same time." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
      else if ( (cfgTargetID == 0 ) && (cfgBuddyID == 0))
      {
         std::cerr << "Either --targetid or --mirrorgroupid must be given." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
      else if (cfgBuddyID != 0)
      {
         cfgTargetID = cfgBuddyID;
      }

      if(ModeHelper::checkInvalidArgs(cfg) )
         return APPCODE_INVALID_CONFIG;

      retVal = getStatsStorage(cfgTargetID, cfgBuddyID != 0);
   }
   else // nodeType == NODETYPE_Meta
   {
      if (cfgNodeID != 0 && cfgBuddyID != 0)
      {
         std::cerr << "--nodeid and --mirrorgroupid cannot be used at the same time." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
      else if (cfgNodeID == 0 && cfgBuddyID == 0)
      {
         std::cerr << "Either --nodeid or --mirrorgroupid must be given." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
      else if (cfgBuddyID != 0)
      {
         cfgNodeID = cfgBuddyID;
      }

      retVal = getStatsMeta(cfgNodeID, cfgBuddyID != 0);
   }

   return retVal;
}

void ModeResyncStats::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --nodetype=<nodetype>          The node type (metadata, storage)." << std::endl;
   std::cout << "  --targetid=<targetID>          The ID of the secondary storage target that is" << std::endl;
   std::cout << "                                 being resynced." << std::endl;
   std::cout << "  --nodeid=<nodeID>              The ID of the secondary metadata node that is" << std::endl;
   std::cout << "                                 being resynced." << std::endl;
   std::cout << "  --mirrorgroupid=<buddyGroupID> The ID of the mirror buddy group that is being " << std::endl;
   std::cout << "                                 resynced." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " Retrieves statistics on a running resync" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Retrieve statistics of target with ID \"10\"" << std::endl;
   std::cout << "  # beegfs-ctl --resyncstats --targetid=10" << std::endl;
}

/**
 * @return APPCODE_...
 */
int ModeResyncStats::getStatsStorage(uint16_t syncToTargetID, bool isBuddyGroupID)
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();

   const auto group = buddyGroupMapper->getMirrorBuddyGroup(isBuddyGroupID
         ? syncToTargetID
         : buddyGroupMapper->getBuddyGroupID(syncToTargetID));
   if (group.firstTargetID == 0) {
      std::cerr << "Could not find buddy group "
         << (isBuddyGroupID ? "" : "of target ")
         << syncToTargetID
         << "." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   const auto syncFromTargetID = group.firstTargetID;
   StorageBuddyResyncJobStatistics jobStats;

   // get the storage node corresponding to the ID
   FhgfsOpsErr getStatsRes = ModeHelper::getStorageBuddyResyncStats(syncFromTargetID, jobStats);

   if (getStatsRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Statistics could not be retrieved; primary target ID: " << syncFromTargetID
         << "; secondary target ID: " << syncToTargetID;
      std::cerr << "; Error: " << getStatsRes << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   printStats(jobStats);

   return APPCODE_NO_ERROR;
}

int ModeResyncStats::getStatsMeta(uint16_t nodeID, bool isBuddyGroupID)
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   uint16_t syncFromNodeID;
   uint16_t syncToNodeID;


   if (isBuddyGroupID)
   {
      MirrorBuddyGroup mbg = buddyGroupMapper->getMirrorBuddyGroup(nodeID);
      syncFromNodeID = mbg.firstTargetID;
      syncToNodeID = mbg.secondTargetID;
   }
   else
   {
      bool primary;
      uint16_t buddyGroupID = buddyGroupMapper->getBuddyGroupID(nodeID, &primary);

      if (primary)
      {
         syncFromNodeID = nodeID;
         syncToNodeID = buddyGroupMapper->getSecondaryTargetID(buddyGroupID);
      }
      else
      {
         syncToNodeID = nodeID;
         syncFromNodeID = buddyGroupMapper->getPrimaryTargetID(buddyGroupID);
      }
   }

   MetaBuddyResyncJobStatistics jobStats;
   FhgfsOpsErr getStatsRes = ModeHelper::getMetaBuddyResyncStats(syncFromNodeID, jobStats);

   if (getStatsRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Statistics could not be retrieved; primary node ID: " << syncFromNodeID
         << "; secondary node ID: " << syncToNodeID;
      std::cerr << "; Error: " << getStatsRes << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   printStats(jobStats);

   return APPCODE_NO_ERROR;
}

void ModeResyncStats::printStats(StorageBuddyResyncJobStatistics& jobStats)
{
   printStats((BuddyResyncJobStatistics)jobStats); // NOLINT(cppcoreguidelines-slicing)
   std::cout << "# of discovered dirs: " << jobStats.getDiscoveredDirs() << std::endl;
   std::cout << "# of discovered files: " << jobStats.getDiscoveredFiles() << std::endl;
   std::cout << "# of dir sync candidates: " << jobStats.getMatchedDirs() << std::endl;
   std::cout << "# of file sync candidates: " << jobStats.getMatchedFiles() << std::endl;
   std::cout << "# of synced dirs: " << jobStats.getSyncedDirs() << std::endl;
   std::cout << "# of synced files: " << jobStats.getSyncedFiles() << std::endl;
   std::cout << "# of dir sync errors: " << jobStats.getErrorDirs () << std::endl;
   std::cout << "# of file sync errors: " << jobStats.getErrorFiles() << std::endl;
}

void ModeResyncStats::printStats(MetaBuddyResyncJobStatistics& jobStats)
{
   printStats((BuddyResyncJobStatistics)jobStats); // NOLINT(cppcoreguidelines-slicing)

   std::cout << "# of discovered dirs: " << jobStats.getDirsDiscovered() << std::endl;
   std::cout << "# of discovery errors: " << jobStats.getGatherErrors() << std::endl;
   std::cout << "# of synced dirs: " << jobStats.getDirsSynced() << std::endl;
   std::cout << "# of synced files: " << jobStats.getFilesSynced() << std::endl;
   std::cout << "# of dir sync errors: " << jobStats.getDirErrors() << std::endl;
   std::cout << "# of file sync errors: " << jobStats.getFileErrors() << std::endl;
   std::cout << "# of client sessions to sync: " << jobStats.getSessionsToSync() << std::endl;
   std::cout << "# of synced client sessions: " << jobStats.getSessionsSynced() << std::endl;
   std::cout << "session sync error: " << (jobStats.getSessionSyncErrors() ? "Yes" : "No") << std::endl;
   std::cout << "# of modification objects synced: " << jobStats.getModObjectsSynced() << std::endl;
   std::cout << "# of modification sync errors: " << jobStats.getModSyncErrors() << std::endl;
}

void ModeResyncStats::printStats(BuddyResyncJobStatistics jobStats)
{
   time_t startTimeSecs = (time_t)jobStats.getStartTime();
   time_t endTimeSecs = (time_t)jobStats.getEndTime();
   std::cout << "Job state: " << jobStatusToStr(jobStats.getState()) << std::endl;
   if (startTimeSecs)
      std::cout << "Job start time: " << ctime(&startTimeSecs);
   if (endTimeSecs)
      std::cout << "Job end time: " << ctime(&endTimeSecs);
}

std::string ModeResyncStats::jobStatusToStr(BuddyResyncJobState status)
{
   switch (status)
   {
      case BuddyResyncJobState_NOTSTARTED: return "Not started";
      case BuddyResyncJobState_RUNNING: return "Running";
      case BuddyResyncJobState_SUCCESS: return "Completed successfully";
      case BuddyResyncJobState_INTERRUPTED: return "Interrupted";
      case BuddyResyncJobState_FAILURE: return "Failed";
      case BuddyResyncJobState_ERRORS: return "Completed with errors";
      default: return "Unknown";
   }
}
