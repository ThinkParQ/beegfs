#include <app/App.h>
#include <common/Common.h>

#include <common/net/message/storage/quota/GetDefaultQuotaMsg.h>
#include <common/net/message/storage/quota/GetDefaultQuotaRespMsg.h>
#include <common/nodes/StoragePoolStore.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/storage/quota/Quota.h>
#include <common/system/System.h>
#include <program/Program.h>

#include "ModeGetQuotaInfo.h"


//the arguments of the mode getQuota
#define MODEGETQUOTAINFO_ARG_UID                "--uid"
#define MODEGETQUOTAINFO_ARG_GID                "--gid"
#define MODEGETQUOTAINFO_ARG_ALL                "--all"
#define MODEGETQUOTAINFO_ARG_CSV                "--csv"
#define MODEGETQUOTAINFO_ARG_LIST               "--list"
#define MODEGETQUOTAINFO_ARG_RANGE              "--range"
#define MODEGETQUOTAINFO_ARG_WITHZERO           "--withzero"
#define MODEGETQUOTAINFO_ARG_WITHSYSTEM         "--withsystem"
#define MODEGETQUOTAINFO_ARG_DEFAULT            "--defaultlimits"
#define MODEGETQUOTAINFO_ARG_STORAGEPOOL        "--storagepoolid"

static const int MODEGETQUOTAINFO_RETVAL_USEDQUOTA_SUCCESS =  0;
static const int MODEGETQUOTAINFO_RETVAL_USEDQUOTA_INVALID = -1;
static const int MODEGETQUOTAINFO_RETVAL_USEDQUOTA_EMPTY   = -2;



int ModeGetQuotaInfo::execute()
{
   NodeList storageNodesList;

   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   ModeHelper::printEnterpriseFeatureMsg();

   storageNodes = app->getStorageNodes();
   mgmtNodes = app->getMgmtNodes();
   targetMapper = app->getTargetMapper();
   storagePoolStore = app->getStoragePoolStore();

   bool configRes = checkConfig(cfg);
   if (configRes != APPCODE_NO_ERROR)
      return configRes;


   if(this->cfg.cfgUseAll)
   {
      if(this->cfg.cfgType == QuotaDataType_USER)
         System::getAllUserIDs(&this->cfg.cfgIDList, !this->cfg.cfgWithSystemUsersGroups);
      else
         System::getAllGroupIDs(&this->cfg.cfgIDList, !this->cfg.cfgWithSystemUsersGroups);
   }

   //remove all duplicated IDs, the list::unique() needs a sorted list
   if (!this->cfg.cfgIDList.empty())
   {
      this->cfg.cfgIDList.sort();
      this->cfg.cfgIDList.unique();
   }

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   // if storage pool ID is given, check if it exists
   if (this->cfg.cfgStoragePoolId != StoragePoolStore::INVALID_POOL_ID)
   {
      bool poolExists = storagePoolStore->poolExists(this->cfg.cfgStoragePoolId);

      if (!poolExists)
      {
         std::cerr << "Storage pool ID doesn't exist: " << this->cfg.cfgStoragePoolId << "."
         << std::endl;

         return APPCODE_INVALID_CONFIG;
      }

      bool result = queryAndPrintQuota(mgmtNode, this->cfg.cfgStoragePoolId, false);

      if (!result)
         return APPCODE_RUNTIME_ERROR;
   }
   else
   {  // no pool given => show all
      StoragePoolPtrVec storagePools = storagePoolStore->getPoolsAsVec();
      for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
      {
         bool result = queryAndPrintQuota(mgmtNode, (*iter)->getId(), true);

         if (!result)
            return APPCODE_RUNTIME_ERROR;
      }
   }

   return APPCODE_NO_ERROR;
}

bool ModeGetQuotaInfo::queryAndPrintQuota(const NodeHandle& mgmtNode, StoragePoolId storagePoolId,
   bool printHeader)
{
   QuotaDataMapForTarget usedQuotaDataResults;
   QuotaDataMapForTarget quotaDataLimitsResults;
   QuotaDefaultLimits defaultLimits;

   if (printHeader)
   {
      std::string poolDesc = storagePoolStore->getPool(storagePoolId)->getDescription();

      std::cout << std::endl
                << "Quota information for storage pool " << poolDesc
                << " (ID: " << storagePoolId << "):" << std::endl << std::endl;
   }

   // query the default quota
   if(!requestDefaultLimits(*mgmtNode, defaultLimits, storagePoolId) )
      return false;

   // if only default quota required, print the default quota and abort
   if(this->cfg.cfgDefaultLimits)
   {
      printDefaultQuotaLimits(defaultLimits);
      return true;
   }

   QuotaInodeSupport quotaInodeSupport = QuotaInodeSupport_UNKNOWN;

   // storage pools filtering will only work if we query each target individually on the server
   this->cfg.cfgTargetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET;

   // query the used quota data
   if (!this->requestQuotaDataAndCollectResponses(storageNodes,
       Program::getApp()->getWorkQueue(), &usedQuotaDataResults, targetMapper,
       &quotaInodeSupport, storagePoolStore, storagePoolId))
   {
      std::cerr << "Could not gather quota information from all storage nodes.\n"
              << "Refusing to display incorrect information." << std::endl;
      return false;
   }

   // sum up the quota, as this is not done inside requestQuotaDataAndCollectResponses for this
   // case due to GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET
   QuotaDataMap usedQuotaSum = calculateQuotaSums(usedQuotaDataResults);

   usedQuotaDataResults.insert(
      QuotaDataMapForTargetMapVal(QUOTADATAMAPFORTARGET_ALL_TARGETS_ID, usedQuotaSum));

   // query the quota limits
   if (!this->requestQuotaLimitsAndCollectResponses(mgmtNode, Program::getApp()->getWorkQueue(),
            &quotaDataLimitsResults, targetMapper, storagePoolId))
   {
      return false;
   }

   QuotaDataMapForTargetIter iterUsedQuota = usedQuotaDataResults.find(
      QUOTADATAMAPFORTARGET_ALL_TARGETS_ID);
   QuotaDataMapForTargetIter iterQuotaLimits = quotaDataLimitsResults.find(
      QUOTADATAMAPFORTARGET_ALL_TARGETS_ID);

   if(iterUsedQuota != usedQuotaDataResults.end() &&
      iterQuotaLimits != quotaDataLimitsResults.end() )
   {
      printQuota(&iterUsedQuota->second, &iterQuotaLimits->second, defaultLimits,
         quotaInodeSupport);
      return true;
   }
   else
   {
      std::cerr << "Couldn't receive valid quota data." << std::endl;
      return false;
   }
}

/*
 * checks the arguments from the commandline
 *
 * @param cfg a mstring map with all arguments from the command line
 * @return the error code (APPCODE_...)
 *
 */
int ModeGetQuotaInfo::checkConfig(StringMap* cfg)
{
   StringMapIter iter;

   // check arguments
   { // keep this block at the beginning and don't change the order because csv output and storage
     // pools are supported by the default quota argument and if the default quota argument is
     // found no further arguments must be checked

      // check if storage pool ID is set
      iter = cfg->find(MODEGETQUOTAINFO_ARG_STORAGEPOOL);
      if (iter != cfg->end()) // pool ID is set
      {
         if (!StringTk::isNumeric(iter->second))
         {
            std::cerr << MODEGETQUOTAINFO_ARG_STORAGEPOOL
                      << " must be a numeric value."
                      << std::endl;

            return APPCODE_RUNTIME_ERROR;
         }

         this->cfg.cfgStoragePoolId.fromStr(iter->second);

         cfg->erase(iter);
      }
      else
      {
         this->cfg.cfgStoragePoolId = StoragePoolStore::INVALID_POOL_ID;
      }

      // parse quota argument for raw printing
      iter = cfg->find(MODEGETQUOTAINFO_ARG_CSV);
      if (iter != cfg->end())
      {
         this->cfg.cfgCsv = true;
         cfg->erase(iter);
      }

      // parse default quota selection and abort if more arguments are set
      iter = cfg->find(MODEGETQUOTAINFO_ARG_DEFAULT);
      if(iter != cfg->end() )
      {
         this->cfg.cfgDefaultLimits = true;
         cfg->erase(iter);

         if( ModeHelper::checkInvalidArgs(cfg) )
            return APPCODE_INVALID_CONFIG;

         return APPCODE_NO_ERROR;
      }
   }

   // parse include unused UIDs/GIDs
   iter = cfg->find(MODEGETQUOTAINFO_ARG_WITHZERO);
   if(iter != cfg->end() )
   {
      this->cfg.cfgPrintUnused = true;
      cfg->erase(iter);
   }

   // parse include system user/groups
   iter = cfg->find(MODEGETQUOTAINFO_ARG_WITHSYSTEM);
   if(iter != cfg->end() )
   {
      this->cfg.cfgWithSystemUsersGroups = true;
      cfg->erase(iter);
   }

   // parse quota type argument for user
   iter = cfg->find(MODEGETQUOTAINFO_ARG_UID);
   if (iter != cfg->end())
   {
      if (this->cfg.cfgType != QuotaDataType_NONE)
      {
         std::cerr << "Invalid configuration. Only one of --uid or --gid is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgType = QuotaDataType_USER;
      cfg->erase(iter);
   }


   // parse quota type argument for group
   iter = cfg->find(MODEGETQUOTAINFO_ARG_GID);
   if (iter != cfg->end())
   {
      if (this->cfg.cfgType != QuotaDataType_NONE)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --uid or --gid is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgType = QuotaDataType_GROUP;
      cfg->erase(iter);
   }


   // check if one quota type argument was set
   if (this->cfg.cfgType == QuotaDataType_NONE)
   {
      std::cerr << "Invalid configuration. "
         "One of --uid or --gid is required." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }


   // parse quota argument for query all GIDs or UIDs
   iter = cfg->find(MODEGETQUOTAINFO_ARG_ALL);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseList || this->cfg.cfgUseRange)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --list, or --range is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgUseAll = true;
      cfg->erase(iter);
   }

   // parse quota argument for a list of GIDs or UIDs
   iter = cfg->find(MODEGETQUOTAINFO_ARG_LIST);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseAll || this->cfg.cfgUseRange)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --list, or --range is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgUseList = true;
      cfg->erase(iter);
   }

   // parse quota argument for a range of GIDs or UIDs
   iter = cfg->find(MODEGETQUOTAINFO_ARG_RANGE);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseAll || this->cfg.cfgUseList)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --list, or --range is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgUseRange = true;
      cfg->erase(iter);
   }

   // check if ID, ID list or ID range is given if it is required
   if(cfg->empty() && !this->cfg.cfgUseAll)
   {
      if(this->cfg.cfgUseRange)
         std::cerr << "No ID range specified." << std::endl;
      else
      if(this->cfg.cfgUseList)
         std::cerr << "No ID list specified." << std::endl;
      else
         std::cerr << "No ID specified." << std::endl;

      return APPCODE_INVALID_CONFIG;
   }


   // parse the ID, ID list or ID range if needed
   if(this->cfg.cfgUseRange)
   {
      if(cfg->size() < 2) // 2 for start and end value of the range
      {
         std::cerr << "No ID range specified. "
            "Use a space to separate the first ID and the last ID." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgIDRangeStart = StringTk::strToUInt(cfg->begin()->first);
      cfg->erase(cfg->begin() );

      this->cfg.cfgIDRangeEnd = StringTk::strToUInt(cfg->begin()->first);
      cfg->erase(cfg->begin() );

      if(this->cfg.cfgIDRangeStart > this->cfg.cfgIDRangeEnd)
      {
         unsigned tmpValue = this->cfg.cfgIDRangeStart;
         this->cfg.cfgIDRangeStart = this->cfg.cfgIDRangeEnd;
         this->cfg.cfgIDRangeEnd = tmpValue;
      }
   }
   else
   if(this->cfg.cfgUseList)
   {
      StringList listOfIDs;
      std::string listString = cfg->begin()->first;
      StringTk::explodeEx(listString, ',', true , &listOfIDs);

      for(StringListIter iter = listOfIDs.begin(); iter != listOfIDs.end(); iter++)
      {
         if(StringTk::isNumeric(*iter) )
            this->cfg.cfgIDList.push_back(StringTk::strToUInt(*iter));
         else
         {
            uint newID;
            bool lookupRes = (this->cfg.cfgType == QuotaDataType_USER) ?
               System::getUIDFromUserName(*iter, &newID) :
               System::getGIDFromGroupName(*iter, &newID);

            if(!lookupRes)
            { // user/group name not found
               std::cerr << "Unable to resolve given name: " << *iter << std::endl;
               return APPCODE_INVALID_CONFIG;
            }
            this->cfg.cfgIDList.push_back(newID);
         }
      }

      cfg->erase(cfg->begin() );
   }
   else
   if(!this->cfg.cfgUseAll)
   { // single ID is given
      std::string queryIDStr = cfg->begin()->first;

      if(StringTk::isNumeric(queryIDStr) )
         this->cfg.cfgID = StringTk::strToUInt(queryIDStr);
      else
      {
         bool lookupRes = (this->cfg.cfgType == QuotaDataType_USER) ?
            System::getUIDFromUserName(queryIDStr, &this->cfg.cfgID) :
            System::getGIDFromGroupName(queryIDStr, &this->cfg.cfgID);

         if(!lookupRes)
         { // user/group name not found
            std::cerr << "Unable to resolve given name: " << queryIDStr << std::endl;
            return APPCODE_INVALID_CONFIG;
         }
      }

      cfg->erase(cfg->begin() );
   }


   // print UID/GID with zero values when a single UID/GID is given
   if(!this->cfg.cfgUseRange && !this->cfg.cfgUseAll && !this->cfg.cfgUseList)
   {
      this->cfg.cfgPrintUnused = true;
   }

   //check any wrong/unknown arguments are given
   if( ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;


   return APPCODE_NO_ERROR;
}

bool ModeGetQuotaInfo::requestDefaultLimits(Node& mgmtNode, QuotaDefaultLimits& defaultLimits,
      StoragePoolId storagePoolId)
{
   bool retVal = false;

   GetDefaultQuotaRespMsg* respMsgCast;

   GetDefaultQuotaMsg msg(storagePoolId);

   const auto respMsg = MessagingTk::requestResponse(mgmtNode, msg, NETMSGTYPE_GetDefaultQuotaResp);
   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << mgmtNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (GetDefaultQuotaRespMsg*)respMsg.get();
   defaultLimits.updateLimits(respMsgCast->getDefaultLimits() );

   retVal = true;

err_cleanup:
   return retVal;
}

/*
 * prints the help
 */
void ModeGetQuotaInfo::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  One of these arguments is mandatory:" << std::endl;
   std::cout << "    --uid             Show information for users." << std::endl;
   std::cout << "    --gid             Show information for groups." << std::endl;
   std::cout << "    --defaultlimits   Show the default quota limits for users and groups groups." << std::endl;
   std::cout << std::endl;
   std::cout << "  One of these arguments is mandatory for argument --uid and --gid:" << std::endl;
   std::cout << "    <ID>                  Show quota for this single user or group ID." << std::endl;
   std::cout << "    --all                 Show quota for all known user or group IDs on localhost." << std::endl;
   std::cout << "                          (System users/groups with UID/GID less than 100 or" << std::endl;
   std::cout << "                          shell set to \"nologin\" or \"false\" are filtered.)" << std::endl;
   std::cout << "    --list <list>         Use a comma separated list of user/group IDs." << std::endl;
   std::cout << "    --range <start> <end> Request quota inforamtion for all user/group IDs in this range," << std::endl;
   std::cout << "                          even if no name is associated with the ID. This can reveal" << std::endl;
   std::cout << "                          residual space consumed by files belonging to deleted users/groups." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional for argument --uid and --gid:" << std::endl;
   std::cout << "    --withzero           Print also users/groups that use no disk space. It is " << std::endl;
   std::cout << "                         default for a single UID/GID." << std::endl;
   std::cout << "    --withsystem         Print also system users/groups. It is default for a single" << std::endl;
   std::cout << "                         UID/GID and a list of UIDs/GIDs." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional argument for --uid, --gid and --defaultlimits:" << std::endl;
   std::cout << "    --csv                Print quota information in comma-separated values with no" << std::endl;
   std::cout << "                         units, i.e. bytes." << std::endl;
   std::cout << "    --storagepoolid=<ID> Only display used quota on targets, which belong to storage" << std::endl;
   std::cout << "                         target pool ID." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode collects the user/group quota information for the given UIDs/GIDs" << std::endl;
   std::cout << " from the storage servers and prints it to the console." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for user ID 1000 on storage pool 2." << std::endl;
   std::cout << "  $ beegfs-ctl --getquota --uid 1000 --storagepoolid=2" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for all normal users on storage pool 1." << std::endl;
   std::cout << "  $ beegfs-ctl --getquota --uid --all --storagepoolid=1" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for user IDs 100 and 110 on storage pool 2." << std::endl;
   std::cout << "  $ beegfs-ctl --getquota --uid --list 100,110 --storagepoolid=2" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for group IDs range 1000 to 1500 on all storage "<< std::endl;
   std::cout << " pools." << std::endl;
   std::cout << "  $ beegfs-ctl --getquota --gid --range 1000 1500" << std::endl;
}

/*
 * prints the quota results
 *
 * @param usedQuota a list with the content of all used quota
 * @param quotaLimits a list with the content of all quota limits
 *
 */
void ModeGetQuotaInfo::printQuota(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
   QuotaDefaultLimits& defaultLimits, QuotaInodeSupport quotaInodeSupport)
{
   bool allValid = false;

   // prepare default limits
   std::string defaultLimitSize;
   std::string defaultInodeLimit;
   getDefaultQuotaStrings(defaultLimits, cfg.cfgType, defaultLimitSize, defaultInodeLimit);

   if(this->cfg.cfgCsv)
   {
      // print header vor csv file
      printf("name,id,size,hard,files,hard\n");
   }
   else
   {
      // print header for console output
      printf("      user/group     ||           size          ||    chunk files    \n");
      printf("     name     |  id  ||    used    |    hard    ||  used   |  hard   \n");
      printf("--------------|------||------------|------------||---------|---------\n");
   }

   if(this->cfg.cfgUseAll || this->cfg.cfgUseList)
      allValid = printQuotaForList(usedQuota, quotaLimits, defaultLimitSize, defaultInodeLimit,
         quotaInodeSupport);
   else
   if(this->cfg.cfgUseRange)
      allValid = printQuotaForRange(usedQuota, quotaLimits, defaultLimitSize, defaultInodeLimit,
         quotaInodeSupport);
   else
      allValid = printQuotaForID(usedQuota, quotaLimits, this->cfg.cfgID, defaultLimitSize,
         defaultInodeLimit, quotaInodeSupport);

   if(!allValid)
      std::cerr << std::endl << "Some IDs were skipped because of invalid values. "
         "Check server logs." << std::endl;
}

bool ModeGetQuotaInfo::printQuotaForID(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
   unsigned id, std::string& defaultSizeLimit, std::string& defaultInodeLimit,
   QuotaInodeSupport quotaInodeSupport)
{
   // prepare used quota values
   std::string usedSize;
   std::string usedInodes;
   int retVal = getUsedQuotaStrings(usedQuota, id, quotaInodeSupport, usedSize, usedInodes);

   // skip invalid or empty quota data
   if(retVal == MODEGETQUOTAINFO_RETVAL_USEDQUOTA_INVALID)
   {
      return false;
   }
   else if(retVal == MODEGETQUOTAINFO_RETVAL_USEDQUOTA_EMPTY)
   {
      return true;
   }

   // prepare hard limit values
   std::string hardLimitSize;
   std::string hardLimitInodes;
   getQuotaLimitsStrings(quotaLimits, id, defaultSizeLimit, defaultInodeLimit, hardLimitSize,
      hardLimitInodes);

   // prepare the user or group name
   std::string name;

   if(this->cfg.cfgType == QuotaDataType_USER)
      name = System::getUserNameFromUID(id);
   else
      name = System::getGroupNameFromGID(id);

   if (name.empty())
      name = StringTk::uintToStr(id);


   /*
    * The uses size value looks like not very exact but the calculation form KByte to KiByte and
    * the quota size in used blocks and not file size, makes the difference
    */
   if(this->cfg.cfgCsv)
   {
      printf("%s,%u,%s,%s,%s,%s\n",
         name.c_str(),                                         // name
         id,                                                   // id
         usedSize.c_str(),                                     // used size
         hardLimitSize.c_str(),                                // hard limit size
         usedInodes.c_str(),                                   // used chunks
         hardLimitInodes.c_str() );                            // hard limit chunks
   }
   else
   {
      printf("%14s|%6u||%12s|%12s||%9s|%9s\n",
         name.c_str(),                                         // name
         id,                                                   // id
         usedSize.c_str(),                                     // used size
         hardLimitSize.c_str(),                                // hard limit size
         usedInodes.c_str(),                                   // used chunks
         hardLimitInodes.c_str() );                            // hard limit chunks
   }

   return true;
}

bool ModeGetQuotaInfo::printQuotaForRange(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
   std::string& defaultSizeLimitValue, std::string& defaultInodeLimit,
   QuotaInodeSupport quotaInodeSupport)
{
   bool allValid = true;

   for(unsigned id = this->cfg.cfgIDRangeStart; id <= this->cfg.cfgIDRangeEnd; id++)
   {
      bool valid = printQuotaForID(usedQuota, quotaLimits, id, defaultSizeLimitValue,
         defaultInodeLimit, quotaInodeSupport);
      allValid = valid ? allValid : false;
   }

   return allValid;
}

bool ModeGetQuotaInfo::printQuotaForList(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
   std::string& defaultSizeLimitValue, std::string& defaultInodeLimit,
   QuotaInodeSupport quotaInodeSupport)
{
   bool allValid = true;

   for(UIntListIter id = this->cfg.cfgIDList.begin(); id != this->cfg.cfgIDList.end(); id++)
   {
      bool valid = printQuotaForID(usedQuota, quotaLimits, *id, defaultSizeLimitValue,
         defaultInodeLimit, quotaInodeSupport);
      allValid = valid ? allValid : false;
   }

   return allValid;
}

/*
 * prints the default quota limits
 *
 * @param quotaLimits the default quota limits
 */
void ModeGetQuotaInfo::printDefaultQuotaLimits(QuotaDefaultLimits& quotaLimits)
{
   std::string hardLimitSizeUser;
   std::string hardLimitInodesUser;
   std::string hardLimitSizeGroup;
   std::string hardLimitInodesGroup;

   getDefaultQuotaStrings(quotaLimits, QuotaDataType_USER, hardLimitSizeUser, hardLimitInodesUser);
   getDefaultQuotaStrings(quotaLimits, QuotaDataType_GROUP, hardLimitSizeGroup,
      hardLimitInodesGroup);

   if(cfg.cfgCsv)
   {
      printf("user size limit,user chunk files limit,group size limit,group chunk files\n");
      printf("%s,%s,%s,%s\n",
         hardLimitSizeUser.c_str(), hardLimitInodesUser.c_str(),
         hardLimitSizeGroup.c_str(), hardLimitInodesGroup.c_str());
   }
   else
   {
      printf("user size limit:        %s\n", hardLimitSizeUser.c_str() );
      printf("user chunk files limit: %s\n", hardLimitInodesUser.c_str() );
      printf("group size limit:       %s\n", hardLimitSizeGroup.c_str() );
      printf("group chunk files:      %s\n\n", hardLimitInodesGroup.c_str() );
   }
}

int ModeGetQuotaInfo::getUsedQuotaStrings(QuotaDataMap* usedQuota, unsigned id,
   QuotaInodeSupport quotaInodeSupport, std::string& usedSizeOut, std::string& usedInodesOut)
{
   std::string usedSizeUnit;

   QuotaDataMapIter usedQuotaIter = usedQuota->find(id);
   if(usedQuotaIter != usedQuota->end() )
   {
      // check if used quota data are valid
      if(!usedQuotaIter->second.isValid() )
         return MODEGETQUOTAINFO_RETVAL_USEDQUOTA_INVALID;

      // skip user/group with no space usage
      if(!this->cfg.cfgPrintUnused &&
         !usedQuotaIter->second.getSize() && !usedQuotaIter->second.getInodes() )
         return MODEGETQUOTAINFO_RETVAL_USEDQUOTA_EMPTY;

      if(this->cfg.cfgCsv)
      {
         usedSizeOut = StringTk::doubleToStr(usedQuotaIter->second.getSize(), 0);
      }
      else
      {
         double usedSizeValue = UnitTk::byteToXbyte(usedQuotaIter->second.getSize(), &usedSizeUnit);
         int precision = ( (usedSizeUnit == "Byte") || (usedSizeUnit == "B") ) ? 0 : 2;
         usedSizeOut = StringTk::doubleToStr(usedSizeValue, precision);
         usedSizeOut.append(" ").append(usedSizeUnit);
      }

      if(quotaInodeSupport == QuotaInodeSupport_NO_BLOCKDEVICES)
      {
         usedInodesOut = "-";
      }
      else if(quotaInodeSupport == QuotaInodeSupport_SOME_BLOCKDEVICES)
      {
         usedInodesOut = StringTk::uint64ToStr(usedQuotaIter->second.getInodes() );
         usedInodesOut.append("+");
      }
      else
      {
         usedInodesOut = StringTk::uint64ToStr(usedQuotaIter->second.getInodes() );
      }
   }
   else if(this->cfg.cfgPrintUnused)
   {
      usedSizeOut = "0";

      if (!cfg.cfgCsv)
         usedSizeOut.append(" Byte");

      if(quotaInodeSupport == QuotaInodeSupport_NO_BLOCKDEVICES)
      {
         usedInodesOut = "-";
      }
      else if(quotaInodeSupport == QuotaInodeSupport_SOME_BLOCKDEVICES)
      {
         usedInodesOut = "0+";
      }
      else
      {
         usedInodesOut = "0";
      }
   }
   else
   { // skip user/group with no space usage
      return MODEGETQUOTAINFO_RETVAL_USEDQUOTA_EMPTY;
   }

   return MODEGETQUOTAINFO_RETVAL_USEDQUOTA_SUCCESS;
}

void ModeGetQuotaInfo::getQuotaLimitsStrings(QuotaDataMap* quotaLimits, unsigned id,
   std::string& defaultSizeLimit, std::string& defaultInodeLimit, std::string& limitSizeOut,
   std::string& limitInodesOut)
{
   QuotaDataMapIter limitsIter = quotaLimits->find(id);
   if(limitsIter != quotaLimits->end() )
   {
      // check if a individual size limit is set
      if(limitsIter->second.getSize() )
      { // use the individual size limit
         if(isUnlimtedValue(limitsIter->second.getSize() ) )
         {
            limitSizeOut = "unlimited";
         }
         else if( this->cfg.cfgCsv )
         {
            limitSizeOut = StringTk::doubleToStr(limitsIter->second.getSize(), 0);
         }
         else
         {
            std::string sizeUnit;
            double limitSizeValue = UnitTk::byteToXbyte(limitsIter->second.getSize(), &sizeUnit);
            int precision = ( (sizeUnit == "Byte") || (sizeUnit == "B") ) ? 0 : 2;
            limitSizeOut = StringTk::doubleToStr(limitSizeValue, precision);
            limitSizeOut.append(" ").append(sizeUnit);
         }
      }
      else
      { // individual size limit is not set, use the default size limit
         limitSizeOut = defaultSizeLimit;
      }

      // check if a individual inode limit is set
      if(limitsIter->second.getInodes() )
      {
         if(isUnlimtedValue(limitsIter->second.getInodes() ) )
         {
            limitInodesOut = "unlimited";
         }
         else
         {
            limitInodesOut = StringTk::uint64ToStr(limitsIter->second.getInodes() );
         }
      }
      else
      { // individual inode limit is not set, use the default inode limit
         limitInodesOut = defaultInodeLimit;
      }
   }
   else
   { // ID not found, use default limits
      limitSizeOut = defaultSizeLimit;
      limitInodesOut = defaultInodeLimit;
   }
}

void ModeGetQuotaInfo::getDefaultQuotaStrings(QuotaDefaultLimits& quotaLimits, QuotaDataType type,
   std::string& defaultLimitSizeOut, std::string& defaultLimitInodesOut)
{
   uint64_t limitSizeValueByte = (type == QuotaDataType_USER) ?
      quotaLimits.getDefaultUserQuotaSize() : quotaLimits.getDefaultGroupQuotaSize();
   if(isUnlimtedValue(limitSizeValueByte) )
   {
      defaultLimitSizeOut = "unlimited";
   }
   else
   {
      if(this->cfg.cfgCsv)
      {
         defaultLimitSizeOut = StringTk::uint64ToStr(limitSizeValueByte);
      }
      else
      {
         std::string limitSizeUnit;
         double limitSizeValue = UnitTk::byteToXbyte(limitSizeValueByte, &limitSizeUnit);
         int precision = ( (limitSizeUnit == "Byte") || (limitSizeUnit == "B") ) ? 0 : 2;
         defaultLimitSizeOut = StringTk::doubleToStr(limitSizeValue, precision);
         defaultLimitSizeOut.append(" ").append(limitSizeUnit);
      }
   }

   uint64_t inodeLimitValue = (type == QuotaDataType_USER) ?
      quotaLimits.getDefaultUserQuotaInodes() : quotaLimits.getDefaultGroupQuotaInodes();
   if(isUnlimtedValue(inodeLimitValue) )
   {
      defaultLimitInodesOut = "unlimited";
   }
   else
   {
      defaultLimitInodesOut = StringTk::uint64ToStr(inodeLimitValue);
   }
}

bool ModeGetQuotaInfo::isUnlimtedValue(uint64_t inValue)
{
   return ( (!inValue) || (inValue == std::numeric_limits<uint64_t>::max() ) );
}
