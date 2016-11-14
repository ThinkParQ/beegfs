#include <app/App.h>
#include <common/Common.h>

#include <common/net/message/storage/quota/GetDefaultQuotaMsg.h>
#include <common/net/message/storage/quota/GetDefaultQuotaRespMsg.h>
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

int ModeGetQuotaInfo::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   NodeList storageNodesList;

   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   retVal = checkConfig(cfg);
   if (retVal != APPCODE_NO_ERROR)
      return retVal;


   if(this->cfg.cfgUseAll)
   {
      if(this->cfg.cfgType == QuotaDataType_USER)
         System::getAllUserIDs(&this->cfg.cfgIDList, !this->cfg.cfgWithSystemUsersGroups);
      else
         System::getAllGroupIDs(&this->cfg.cfgIDList, !this->cfg.cfgWithSystemUsersGroups);
   }

   //remove all duplicated IDs, the list::unique() needs a sorted list
   if(this->cfg.cfgIDList.size() > 0)
   {
      this->cfg.cfgIDList.sort();
      this->cfg.cfgIDList.unique();
   }

   QuotaDataMapForTarget usedQuotaDataResults;
   QuotaDataMapForTarget quotaDataLimitsResults;

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   QuotaDefaultLimits defaultLimits;

   // query the default quota
   if(!requestDefaultLimits(*mgmtNode, defaultLimits) )
      return APPCODE_RUNTIME_ERROR;

   // if only default quota required, print the default quota and abort
   if(this->cfg.cfgDefaultLimits)
   {
      printDefaultQuotaLimits(defaultLimits);
      return APPCODE_NO_ERROR;
   }

   // query the used quota data
   QuotaInodeSupport quotaInodeSupport = QuotaInodeSupport_UNKNOWN;
   if (!this->requestQuotaDataAndCollectResponses(mgmtNode, storageNodes, app->getWorkQueue(),
         &usedQuotaDataResults, NULL, false, &quotaInodeSupport) )
      return APPCODE_RUNTIME_ERROR;

   // query the quota limits
   QuotaInodeSupport quotaInodeSupportLimits = QuotaInodeSupport_UNKNOWN;
   if (!this->requestQuotaDataAndCollectResponses(mgmtNode, storageNodes, app->getWorkQueue(),
         &quotaDataLimitsResults, NULL, true, &quotaInodeSupportLimits) )
      return APPCODE_RUNTIME_ERROR;

   QuotaDataMapForTargetIter iterUsedQuota = usedQuotaDataResults.find(
      QUOTADATAMAPFORTARGET_ALL_TARGETS_ID);
   QuotaDataMapForTargetIter iterQuotaLimits = quotaDataLimitsResults.find(
      QUOTADATAMAPFORTARGET_ALL_TARGETS_ID);

   if(iterUsedQuota != usedQuotaDataResults.end() &&
      iterQuotaLimits != quotaDataLimitsResults.end() )
   {
      printQuota(&iterUsedQuota->second, &iterQuotaLimits->second, defaultLimits,
         quotaInodeSupport);
   }
   else
   {
      std::cerr << "Couldn't receive valid quota data." << std::endl;
      retVal = APPCODE_RUNTIME_ERROR;
   }

   return retVal;
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
   { // keep this block at the beginning and don't change the order because csv output is supported
     // by the default quota argument and if the default quota argument is found no further
     // arguments must be checked

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


   // print UID/GID with zero values when a list or a single UID/GID is given
   if(!this->cfg.cfgUseRange && !this->cfg.cfgUseAll)
   {
      this->cfg.cfgPrintUnused = true;
   }


   //check any wrong/unknown arguments are given
   if( ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;


   return APPCODE_NO_ERROR;
}

bool ModeGetQuotaInfo::requestDefaultLimits(Node& mgmtNode, QuotaDefaultLimits& defaultLimits)
{
   bool retVal = false;

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   GetDefaultQuotaRespMsg* respMsgCast;


   GetDefaultQuotaMsg msg;

   // request/response
   commRes = MessagingTk::requestResponse(mgmtNode, &msg, NETMSGTYPE_GetDefaultQuotaResp, &respBuf,
      &respMsg);
   if(!commRes)
   {
      std::cerr << "Communication with server failed: " << mgmtNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (GetDefaultQuotaRespMsg*)respMsg;
   defaultLimits.updateLimits(respMsgCast->getDefaultLimits() );

   retVal = true;

err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

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
   std::cout << "    --all                 Show quota for all user or group IDs on localhost." << std::endl;
   std::cout << "                          (System users/groups with UID/GID less than 100 or" << std::endl;
   std::cout << "                          shell set to \"nologin\" or \"false\" are filtered.)" << std::endl;
   std::cout << "    --list <list>         Use a comma separated list of user/group IDs." << std::endl;
   std::cout << "    --range <start> <end> Use a range of user/group IDs." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional for argument --uid and --gid:" << std::endl;
   std::cout << "    --withzero     Print also users/groups that use no disk space. It is default" << std::endl;
   std::cout << "                   for a single UID/GID and a list of UIDs/GIDs." << std::endl;
   std::cout << "    --withsystem   Print also system users/groups. It is default for a single" << std::endl;
   std::cout << "                   UID/GID and a list of UIDs/GIDs." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional argument for --uid, --gid and --defaultlimits:" << std::endl;
   std::cout << "    --csv          Print quota information in comma-separated values with no" << std::endl; 
   std::cout << "                   units, i.e. bytes." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode collects the user/group quota information for the given UIDs/GIDs" << std::endl;
   std::cout << " from the storage servers and prints it to the console." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for user ID 1000." << std::endl;
   std::cout << "  $ beegfs-ctl --getquota --uid 1000" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for all normal users." << std::endl;
   std::cout << "  $ beegfs-ctl --getquota --uid --all" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for user IDs 100 and 110." << std::endl;
   std::cout << "  $ beegfs-ctl --getquota --uid --list 100,110" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show quota information for group IDs range 1000 to 1500." << std::endl;
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
   std::string defaultLimitSizeUnit;
   uint64_t defaultSizeLimit = (cfg.cfgType == QuotaDataType_USER) ?
      defaultLimits.getDefaultUserQuotaSize() : defaultLimits.getDefaultGroupQuotaSize();
   uint64_t defaultInodeLimit = (cfg.cfgType == QuotaDataType_USER) ?
      defaultLimits.getDefaultUserQuotaInodes() : defaultLimits.getDefaultGroupQuotaInodes();

   if(this->cfg.cfgCsv)
   {
      // prepare default limit size strings for csv file
      defaultLimitSize = StringTk::doubleToStr(defaultSizeLimit, 0);
      defaultLimitSizeUnit = "Byte";

      // print header vor csv file
      printf("name,id,size,hard,files,hard\n");
   }
   else
   {
      // prepare default limit size strings for console output
      double defaultSizeLimitValue = UnitTk::byteToXbyte(defaultSizeLimit, &defaultLimitSizeUnit);
      int precision = ( (defaultLimitSize == "Byte") || (defaultLimitSize == "B") ) ? 0 : 2;
      defaultLimitSize = StringTk::doubleToStr(defaultSizeLimitValue, precision);

      // print header for console output
      printf("      user/group     ||           size          ||    chunk files    \n");
      printf("     name     |  id  ||    used    |    hard    ||  used   |  hard   \n");
      printf("--------------|------||------------|------------||---------|---------\n");
   }

   if(this->cfg.cfgUseAll || this->cfg.cfgUseList)
      allValid = printQuotaForList(usedQuota, quotaLimits, defaultLimitSize, defaultLimitSizeUnit,
         defaultInodeLimit, quotaInodeSupport);
   else
   if(this->cfg.cfgUseRange)
      allValid = printQuotaForRange(usedQuota, quotaLimits, defaultLimitSize, defaultLimitSizeUnit,
         defaultInodeLimit, quotaInodeSupport);
   else
      allValid = printQuotaForID(usedQuota, quotaLimits, this->cfg.cfgID, defaultLimitSize,
         defaultLimitSizeUnit, defaultInodeLimit, quotaInodeSupport);

   if(!allValid)
      std::cerr << std::endl << "Some IDs were skipped because of invalid values. "
         "Check server logs." << std::endl;
}

bool ModeGetQuotaInfo::printQuotaForID(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
   unsigned id, std::string& defaultSizeLimitValue, std::string& defaultSizeLimitUnit,
   uint64_t defaultInodeLimit, QuotaInodeSupport quotaInodeSupport)
{
   // prepare used quota values
   std::string usedSize;
   std::string usedSizeUnit;
   std::string usedInodes;

   QuotaDataMapIter usedQuotaIter = usedQuota->find(id);
   if(usedQuotaIter != usedQuota->end() )
   {
      // check if used quota data are valid
      if(!usedQuotaIter->second.isValid() )
         return false;

      // skip user/group with no space usage
      if(!this->cfg.cfgPrintUnused &&
         !usedQuotaIter->second.getSize() && !usedQuotaIter->second.getInodes() )
         return true;

      if(this->cfg.cfgCsv)
      {
         usedSize = StringTk::doubleToStr(usedQuotaIter->second.getSize(), 0);
         usedSizeUnit = "Byte";
      }
      else
      {
         double usedSizeValue = UnitTk::byteToXbyte(usedQuotaIter->second.getSize(), &usedSizeUnit);
         int precision = ( (usedSizeUnit == "Byte") || (usedSizeUnit == "B") ) ? 0 : 2;
         usedSize = StringTk::doubleToStr(usedSizeValue, precision);
      }

      if(quotaInodeSupport == QuotaInodeSupport_NO_BLOCKDEVICES)
      {
         usedInodes = "-";
      }
      else if(quotaInodeSupport == QuotaInodeSupport_SOME_BLOCKDEVICES)
      {
         usedInodes = StringTk::uint64ToStr(usedQuotaIter->second.getInodes() );
         usedInodes.append("+");
      }
      else
      {
         usedInodes = StringTk::uint64ToStr(usedQuotaIter->second.getInodes() );
      }
   }
   else
   if(this->cfg.cfgPrintUnused)
   {
      usedSize = "0";
      usedSizeUnit = "Byte";

      if(quotaInodeSupport == QuotaInodeSupport_NO_BLOCKDEVICES)
      {
         usedInodes = "-";
      }
      else if(quotaInodeSupport == QuotaInodeSupport_SOME_BLOCKDEVICES)
      {
         usedInodes = "0+";
      }
      else
      {
         usedInodes = "0";
      }
   }
   else // skip user/group with no space usage
      return true;


   // prepare hard limit values
   std::string hardLimitSize;
   std::string hardLimitSizeUnit;
   unsigned long long hardLimitInodes;

   QuotaDataMapIter limitsIter = quotaLimits->find(id);
   if(limitsIter != quotaLimits->end() )
   {
      // check if a individual size limit is set
      if(limitsIter->second.getSize() )
      { // use the individual size limit
         if( this->cfg.cfgCsv )
         {
            hardLimitSize = StringTk::doubleToStr(limitsIter->second.getSize(), 0);
            hardLimitSizeUnit = "Byte";
         }
         else
         {
            double hardLimitSizeValue = UnitTk::byteToXbyte(limitsIter->second.getSize(),
               &hardLimitSizeUnit);
            int precision = ( (hardLimitSizeUnit == "Byte") || (hardLimitSizeUnit == "B") ) ? 0 : 2;
            hardLimitSize = StringTk::doubleToStr(hardLimitSizeValue, precision);
         }
      }
      else
      { // use the default size limit
         hardLimitSize = defaultSizeLimitValue;
         hardLimitSizeUnit = defaultSizeLimitUnit;
      }

      hardLimitInodes = limitsIter->second.getInodes() ? limitsIter->second.getInodes() :
         defaultInodeLimit;
   }
   else
   {
      hardLimitSize = defaultSizeLimitValue;
      hardLimitSizeUnit = defaultSizeLimitUnit;

      hardLimitInodes = defaultInodeLimit;
   }


   // prepare the user or group name
   std::string name;

   if(this->cfg.cfgType == QuotaDataType_USER)
      name = System::getUserNameFromUID(id);
   else
      name = System::getGroupNameFromGID(id);

   if(name.size() == 0)
      name = StringTk::uintToStr(id);


   /*
    * The uses size value looks like not very exact but the calculation form KByte to KiByte and
    * the quota size in used blocks and not file size, makes the difference
    */
   if(this->cfg.cfgCsv)
   {
      printf("%s,%u,%s,%s,%s,%qu\n",
         name.c_str(),                                         // name
         id,                                                   // id
         usedSize.c_str(),                                     // used (blocks)
         hardLimitSize.c_str(),                                // hard limit (blocks)
         usedInodes.c_str(),                                   // used (inodes)
         hardLimitInodes);                                     // hard limit (inodes)
   }
   else
   {
      printf("%14s|%6u||%7s %-4s|%7s %-4s||%9s|%9qu\n",
         name.c_str(),                                         // name
         id,                                                   // id
         usedSize.c_str(),                                     // used (blocks)
         usedSizeUnit.c_str(),                                 // unit (used blocks)
         hardLimitSize.c_str(),                                // hard limit (blocks)
         hardLimitSizeUnit.c_str(),                            // unit (hard limit)
         usedInodes.c_str(),                                   // used (inodes)
         hardLimitInodes);                                     // hard limit (inodes)
   }

   return true;
}

bool ModeGetQuotaInfo::printQuotaForRange(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
   std::string& defaultSizeLimitValue, std::string& defaultSizeLimitUnit,
   uint64_t defaultInodeLimit, QuotaInodeSupport quotaInodeSupport)
{
   bool allValid = false;

   for(unsigned id = this->cfg.cfgIDRangeStart; id <= this->cfg.cfgIDRangeEnd; id++)
      allValid = printQuotaForID(usedQuota, quotaLimits, id, defaultSizeLimitValue,
         defaultSizeLimitUnit, defaultInodeLimit, quotaInodeSupport);

   return allValid;
}

bool ModeGetQuotaInfo::printQuotaForList(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
   std::string& defaultSizeLimitValue, std::string& defaultSizeLimitUnit,
   uint64_t defaultInodeLimit, QuotaInodeSupport quotaInodeSupport)
{
   bool allValid = false;

   for(UIntListIter id = this->cfg.cfgIDList.begin(); id != this->cfg.cfgIDList.end(); id++)
      allValid = printQuotaForID(usedQuota, quotaLimits, *id, defaultSizeLimitValue,
         defaultSizeLimitUnit, defaultInodeLimit, quotaInodeSupport);

   return allValid;
}

/*
 * prints the default quota limits
 *
 * @param quotaLimits the default quota limits
 */
void ModeGetQuotaInfo::printDefaultQuotaLimits(QuotaDefaultLimits& quotaLimits)
{
   std::string hardLimitSizeUnitUser;
   double hardLimitSizeValueUser = UnitTk::byteToXbyte(quotaLimits.getDefaultUserQuotaSize(),
      &hardLimitSizeUnitUser);
   int precisionUser = ( (hardLimitSizeUnitUser == "Byte") || (hardLimitSizeUnitUser == "B") ) ?
      0 : 2;
   std::string hardLimitSizeUser = StringTk::doubleToStr(hardLimitSizeValueUser, precisionUser);
   unsigned long long hardLimitInodesUser = quotaLimits.getDefaultUserQuotaInodes();

   std::string hardLimitSizeUnitGroup;
   double hardLimitSizeValueGroup = UnitTk::byteToXbyte(quotaLimits.getDefaultGroupQuotaSize(),
      &hardLimitSizeUnitGroup);
   int precisionGroup = ( (hardLimitSizeUnitGroup == "Byte") || (hardLimitSizeUnitGroup == "B") ) ?
      0 : 2;
   std::string hardLimitSizeGroup = StringTk::doubleToStr(hardLimitSizeValueGroup, precisionGroup);
   unsigned long long hardLimitInodesGroup = quotaLimits.getDefaultGroupQuotaInodes() ;

   if(cfg.cfgCsv)
   {
      printf("user size limit,user chunk files limit,group size limit,group chunk files\n");
      printf("%s%s,%qu,%s%s,%qu\n",
         hardLimitSizeUser.c_str(), hardLimitSizeUnitUser.c_str(),
         hardLimitInodesUser,
         hardLimitSizeGroup.c_str(), hardLimitSizeUnitGroup.c_str(),
         hardLimitInodesGroup);
   }
   else
   {
      printf("user size limit: %s%s\n", hardLimitSizeUser.c_str(), hardLimitSizeUnitUser.c_str() );
      printf("user chunk files limit: %qu\n", hardLimitInodesUser);
      printf("group size limit: %s%s\n", hardLimitSizeGroup.c_str(),
         hardLimitSizeUnitGroup.c_str() );
      printf("group chunk files: %qu\n\n", hardLimitInodesGroup);
   }
}
