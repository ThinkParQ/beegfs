#include <app/App.h>
#include <common/Common.h>

#include <common/net/message/storage/quota/SetDefaultQuotaMsg.h>
#include <common/net/message/storage/quota/SetDefaultQuotaRespMsg.h>
#include <common/net/message/storage/quota/SetQuotaRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/system/System.h>
#include <program/Program.h>
#include "ModeSetQuota.h"
#include "modes/modehelpers/ModeHelper.h"



//the arguments of the mode setQuota
#define MODESETQUOTA_ARG_UID                 "--uid"
#define MODESETQUOTA_ARG_GID                 "--gid"
#define MODESETQUOTA_ARG_ALL                 "--all"
#define MODESETQUOTA_ARG_LIST                "--list"
#define MODESETQUOTA_ARG_RANGE               "--range"
#define MODESETQUOTA_ARG_FILE                "--file"
#define MODESETQUOTA_ARG_SIZE_LIMIT          "--sizelimit"
#define MODESETQUOTA_ARG_INODE_LIMIT         "--inodelimit"
#define MODESETQUOTA_ARG_DEFAULT             "--default"
#define MODESETQUOTA_ARG_STORAGEPOOLID       "--storagepoolid"

#define MODESETQUOTA_ARGVALUE_SIZE_INODE_RESET          "reset"
#define MODESETQUOTA_ARGVALUE_SIZE_INODE_UNLIMITED      "unlimited"

#define MODESETQUOTA_ERR_OPEN_FILE_FAILED   (-1)
#define MODESETQUOTA_ERR_NONE                0
#define MODESETQUOTA_ERR_FAILD_LINES         1 // positive values are the number of failed lines

int ModeSetQuota::execute()
{
   int retVal = APPCODE_NO_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   ModeHelper::printEnterpriseFeatureMsg();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   retVal = checkConfig(cfg);
   if (retVal != APPCODE_NO_ERROR)
      return retVal;

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   // create the quota data with the limits
   if(this->cfg.cfgUseAll)
   { // collect all available users and add to the ID list, the quota data will be created later
      if(this->cfg.cfgType == QuotaDataType_USER)
         System::getAllUserIDs(&this->cfg.cfgIDList, true);
      else
         System::getAllGroupIDs(&this->cfg.cfgIDList, true);
   }
   else
   if(this->cfg.cfgUseFile)
   { // read all quota limits from file and add to limits list
      int readFileRetVal = readFromFile(this->cfg.cfgFilePath, this->cfg.cfgType);

      if(readFileRetVal == MODESETQUOTA_ERR_OPEN_FILE_FAILED)
         return APPCODE_RUNTIME_ERROR;
      if(readFileRetVal >= MODESETQUOTA_ERR_FAILD_LINES)
         std::cerr << readFileRetVal << " lines of file " << this->cfg.cfgFilePath
         << " are not valid." << mgmtNode->getID() << std::endl;
      if(this->quotaLimitsList.empty())
      {
         std::cerr << "Couldn't generate quota limits from file: " << this->cfg.cfgFilePath
            << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }
   else
   if(this->cfg.cfgUseRange)
   { // convert the ID range into the quota data map
      convertIDRangeToQuotaDataList();
   }
   else
   if(this->cfg.cfgDefaultLimits)
   { // default limit for users or groups
      QuotaData quota(0, this->cfg.cfgType);
      quota.setQuotaData(this->cfg.cfgSizeLimit, this->cfg.cfgInodeLimit),
      this->quotaLimitsList.push_back(quota);
   }
   else
   { // single user ID or group ID is given
      QuotaData quota(this->cfg.cfgID, this->cfg.cfgType);
      quota.setQuotaData(this->cfg.cfgSizeLimit, this->cfg.cfgInodeLimit),
      this->quotaLimitsList.push_back(quota);
   }

   //remove all duplicated IDs, the list::unique() needs a sorted list and convert to quota data map
   if (!this->cfg.cfgIDList.empty())
   {
      this->cfg.cfgIDList.sort();
      this->cfg.cfgIDList.unique();
      convertIDListToQuotaDataList();
   }

   if(this->cfg.cfgDefaultLimits && !this->uploadDefaultQuotaLimitsAndCollectResponses(*mgmtNode) )
      retVal = APPCODE_RUNTIME_ERROR;
   else if (!this->uploadQuotaLimitsAndCollectResponses(*mgmtNode))
      retVal = APPCODE_RUNTIME_ERROR;

   return retVal;
}

/*
 * checks the arguments from the commandline
 *
 * @param cfg a mstring map with all arguments from the command line
 * @return the error code (APPCODE_...)
 *
 */
int ModeSetQuota::checkConfig(StringMap* cfg)
{
   StringMapIter iter;
   bool quotaSizeLimitGiven = false; // variable required, because 0 is a valid value (unlimited)
   bool quotaInodeLimitGiven = false; // variable required, because 0 is a valid value (unlimited)


   // check arguments

   // parse quota type argument for user
   iter = cfg->find(MODESETQUOTA_ARG_UID);
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
   iter = cfg->find(MODESETQUOTA_ARG_GID);
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

   // check if storage pool id was set
   iter = cfg->find(MODESETQUOTA_ARG_STORAGEPOOLID);
   if (iter != cfg->end())
   {
      this->cfg.cfgStoragePoolId.fromStr(iter->second);

      cfg->erase(iter);
   }
   else
   {
      std::cout << "Using default storage pool (" << this->cfg.cfgStoragePoolId << ")" << std::endl;
   }

   // parse quota argument for all GIDs or UIDs
   iter = cfg->find(MODESETQUOTA_ARG_ALL);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseFile || this->cfg.cfgUseList || this->cfg.cfgUseRange ||
         this->cfg.cfgDefaultLimits)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --file, --list, --range or --default is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgUseAll = true;
      cfg->erase(iter);
   }

   // parse quota argument to read quota limits from file of UIDs or GIDs
   iter = cfg->find(MODESETQUOTA_ARG_FILE);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseAll || this->cfg.cfgUseList || this->cfg.cfgUseRange ||
         this->cfg.cfgDefaultLimits)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --file, --list, --range or --default is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgUseFile = true;
      this->cfg.cfgFilePath = iter->second;
      cfg->erase(iter);
   }

   // parse quota argument for a list of quota limits of GIDs or UIDs
   iter = cfg->find(MODESETQUOTA_ARG_LIST);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseAll || this->cfg.cfgUseFile || this->cfg.cfgUseRange ||
         this->cfg.cfgDefaultLimits)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --file, --list, --range or --default is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgUseList = true;
      cfg->erase(iter);
   }

   // parse quota argument for a range of quota limits of GIDs or UIDs
   iter = cfg->find(MODESETQUOTA_ARG_RANGE);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseAll || this->cfg.cfgUseFile || this->cfg.cfgUseList ||
         this->cfg.cfgDefaultLimits)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --file, --list, --range or --default is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgUseRange = true;
      cfg->erase(iter);
   }

   // parse quota argument for the default limits of GIDs or UIDs
   iter = cfg->find(MODESETQUOTA_ARG_DEFAULT);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseAll || this->cfg.cfgUseFile || this->cfg.cfgUseList ||
         this->cfg.cfgUseRange)
      {
         std::cerr << "Invalid configuration. "
            "Only one of --all, --file, --list, --range or --default is allowed." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfg.cfgDefaultLimits = true;
      cfg->erase(iter);
   }


   // parse quota size limit argument
   iter = cfg->find(MODESETQUOTA_ARG_SIZE_LIMIT);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseFile)
      {
         std::cerr << "Invalid configuration. "
            "The arguments --sizelimit is not required when --file is given." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if(UnitTk::isValidHumanString(iter->second))
      {
         this->cfg.cfgSizeLimit = UnitTk::strHumanToInt64(iter->second.c_str());
      }
      else if(iter->second == MODESETQUOTA_ARGVALUE_SIZE_INODE_RESET)
      {
         this->cfg.cfgSizeLimit = 0;
      }
      else if(iter->second == MODESETQUOTA_ARGVALUE_SIZE_INODE_UNLIMITED)
      {
         this->cfg.cfgSizeLimit = std::numeric_limits<uint64_t>::max();
      }
      else
      {
         std::cerr << "Invalid configuration. "
            "The quota size limit is not valid." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      quotaSizeLimitGiven = true;
      cfg->erase(iter);
   }

   // parse quota inode limit argument
   iter = cfg->find(MODESETQUOTA_ARG_INODE_LIMIT);
   if (iter != cfg->end())
   {
      if(this->cfg.cfgUseFile)
      {
         std::cerr << "Invalid configuration. "
            "The arguments --inodelimit is not required when --file is given." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      if (StringTk::isNumeric(iter->second) )
      {
         this->cfg.cfgInodeLimit = StringTk::strToUInt64(iter->second.c_str() );
      }
      else if(iter->second == MODESETQUOTA_ARGVALUE_SIZE_INODE_RESET)
      {
         this->cfg.cfgInodeLimit = 0;
      }
      else if(iter->second == MODESETQUOTA_ARGVALUE_SIZE_INODE_UNLIMITED)
      {
         this->cfg.cfgInodeLimit = std::numeric_limits<uint64_t>::max();
      }
      else
      {
         std::cerr << "Invalid configuration. "
            "The quota inode limit is not a numeric value." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      quotaInodeLimitGiven = true;
      cfg->erase(iter);
   }

   // check if the quota limit arguments are set
   if (!this->cfg.cfgUseFile && !(quotaSizeLimitGiven && quotaInodeLimitGiven) )
   {
      std::cerr << "Invalid configuration. "
         "The arguments --sizelimit and --inodelimit are required." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }


   // check if quota limit, quota limit list or quota limit range is given if it is required
   if(cfg->empty() && !(this->cfg.cfgUseAll || this->cfg.cfgUseFile || this->cfg.cfgDefaultLimits) )
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

   // parse the quota limit, quota limit list or quota limit range if needed
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

      cfg->erase(cfg->begin() );
   }
   else
   if(!this->cfg.cfgUseAll && !this->cfg.cfgUseFile && !this->cfg.cfgDefaultLimits)
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


   //check any wrong/unknown arguments are given
   if( ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   return APPCODE_NO_ERROR;
}

/*
 * prints the help
 */
void ModeSetQuota::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  One of these arguments is mandatory:" << std::endl;
   std::cout << "    --uid       Set quota limits for users." << std::endl;
   std::cout << "    --gid       Set quota limits for groups." << std::endl;
   std::cout << std::endl;
   std::cout << "  One of these arguments is mandatory:" << std::endl;
   std::cout << "    <ID>                  Set quota limits for this single user or group ID." << std::endl;
   std::cout << "    --all                 Set quota limits for all user or group IDs on localhost." << std::endl;
   std::cout << "                          (System users/groups with UID/GID less than 100 or" << std::endl;
   std::cout << "                          shell set to \"nologin\" or \"false\" are filtered.)" << std::endl;
   std::cout << "    --file=<path>         Read the quota limits from a file and set the quota limits." << std::endl;
   std::cout << "                          A csv file is requires each line is a user or a group." << std::endl;
   std::cout << "                          Line format: ID/name,size limit,inode limit" << std::endl;
   std::cout << "    --list <list>         Use a comma separated list of user/group IDs." << std::endl;
   std::cout << "    --range <start> <end> Use a range of user/group IDs." << std::endl;
   std::cout << "    --default             Set the default quota limits for users or groups." << std::endl;
   std::cout << std::endl;
   std::cout << "  Mandatory if not imported via --file:" << std::endl;
   std::cout << "    --sizelimit=<size>    The diskspace limit for the users/groups." << std::endl;
   std::cout << "    --inodelimit=<count>  The inodes (chunk files) limit for the users/groups." << std::endl;
   std::cout << std::endl;
   std::cout << "    --storagepoolid=<ID>  Modify quota limits for given storage pool. (Default=1)" << std::endl;
   std::cout << std::endl;
   std::cout << "NOTE:" << std::endl;
   std::cout << "Without any configuration the limits are set to unlimited. If a default quota" << std::endl;
   std::cout << "limit is set by the beegfs-ctl this quota limits are used. User and group" << std::endl;
   std::cout << "quota limits has a higher priority then the default quota limits. When both" << std::endl;
   std::cout << "user and group quota limits are defined, the lower limit takes precedence for"  << std::endl;
   std::cout << "quota enforcement. " << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode sets the size and inodes (chunk files) quota limits for users and" << std::endl;
   std::cout << " groups." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set quota size limit to 50 MiB and 50 chunk files for user ID 1000 on." << std::endl;
   std::cout << "          storage pool 2." << std::endl;
   std::cout << "  # beegfs-ctl --setquota --sizelimit=50M --inodelimit=50 --storagepoolid=2 \\" << std::endl;
   std::cout << "  --uid 1000" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set group quota limits given in file /tmp/quota_limits.txt on storage" << std::endl;
   std::cout << "          pool 1." << std::endl;
   std::cout << "  # beegfs-ctl --setquota --gid --file=/tmp/quota_limits.txt --storagepoolid=1" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set quota size limit to 20 MiB and 500 chunk files for user IDs 100" << std::endl;
   std::cout << "          and 110 on storage pool 1." << std::endl;
   std::cout << "  # beegfs-ctl --setquota --uid --sizelimit=20M --inodelimit=500 --storagepoolid=1 \\" << std::endl;
   std::cout << "  --list 100,110" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set quota to unlimited diskspace and 5000 chunk files for group IDs" << std::endl;
   std::cout << "          in range 1000 to 1500 on storage pool 2." << std::endl;
   std::cout << "  # beegfs-ctl --setquota --sizelimit=unlimited --inodelimit=5000 --storagepoolid=2 --gid \\" << std::endl;
   std::cout << "  --range 1000 1500" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set default quota for storage pool 2 for users to 20 MiB and 500 chunk " << std::endl;
   std::cout << "          files." << std::endl;
   std::cout << "  # beegfs-ctl --setquota --uid --sizelimit=20M --inodelimit=500 --default \\" << std::endl;
   std::cout << "  --storagepoolid=2" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Reset diskspace and chunk files quota for users ID 100. The default " << std::endl;
   std::cout << "          limits are used afterwards." << std::endl;
   std::cout << "  # beegfs-ctl --setquota --uid --sizelimit=reset --inodelimit=reset 100" << std::endl;
}

/*
 * read the quota limits from a csv file, one line per user or group
 * format: GID/groupname,size limit,inode limit
 * format: UID/username,size limit,inode limit
 *
 * @param pathToFile the path to the csv file with the quota limits
 * @param quotaType the quota type (QuotaDataType)
 * @return error information, MODESETQUOTA_ERR_...
 */
int ModeSetQuota::readFromFile(std::string pathToFile, QuotaDataType quotaType)
{
   int lineNumber = 0;
   int retVal = MODESETQUOTA_ERR_NONE;

   std::ifstream fis(pathToFile.c_str() );
   if(!fis.is_open() || fis.fail() )
   {
      std::cerr << "Failed to load quota limits file: " << pathToFile  << std::endl;;
      return MODESETQUOTA_ERR_OPEN_FILE_FAILED;
   }

   while(!fis.eof() && !fis.fail() )
   {
      StringVector lineList;
      std::string line;

      lineNumber++;

      std::getline(fis, line);
      std::string trimmedLine = StringTk::trim(line);
      if(trimmedLine.length() && (trimmedLine[0] != STORAGETK_FILE_COMMENT_CHAR) )
      {
         StringTk::explodeEx(line, ',', true, &lineList);
         if (lineList.size() == 3)
         {
            unsigned id;
            if(StringTk::isNumeric(lineList[0]) )
               id = StringTk::strToUInt(lineList[0].c_str());
            else
            {
               bool lookupRes = (quotaType == QuotaDataType_USER) ?
                  System::getUIDFromUserName(lineList[0], &id) :
                  System::getGIDFromGroupName(lineList[0], &id);

               if(!lookupRes)
               { // user/group name not found
                  std::cerr << "Unable to resolve given name: " << lineList[0] << " ; Check line "
                     << lineNumber << " of file: " << pathToFile << " ; Format: GID or UID,"
                     "size limit,inode limit ; Example: 2034,500M,10000" << std::endl;

                  retVal++;
                  continue;
               }
            }

            uint64_t sizeLimit = 0;
            uint64_t inodeLimit = 0;

            if(UnitTk::isValidHumanString(lineList[1]) )
            {
               sizeLimit = UnitTk::strHumanToInt64(lineList[1].c_str() );
            }
            else if(lineList[1] == MODESETQUOTA_ARGVALUE_SIZE_INODE_RESET)
            {
               sizeLimit = 0;
            }
            else if(lineList[1] == MODESETQUOTA_ARGVALUE_SIZE_INODE_UNLIMITED)
            {
               sizeLimit = std::numeric_limits<uint64_t>::max();
            }
            else
            { // size limit is a invalid value
               std::cerr << "The value for the size limit is not valid: " << lineList[1] <<
                  " ; Check line " << lineNumber << " of file: " << pathToFile << " Format: GID or "
                  "UID, size limit,inode limit ; Example: 2034,500M,10000 ; "
                  "Example: 2034,unlimited,reset" << std::endl;

               retVal++;
               continue;
            }

            if(StringTk::isNumeric(lineList[2]) )
            {
               inodeLimit = StringTk::strToUInt64(lineList[2].c_str() );
            }
            else if(lineList[2] == MODESETQUOTA_ARGVALUE_SIZE_INODE_RESET)
            {
               inodeLimit = 0;
            }
            else if(lineList[2] == MODESETQUOTA_ARGVALUE_SIZE_INODE_UNLIMITED)
            {
               inodeLimit = std::numeric_limits<uint64_t>::max();
            }
            else
            { // inode limit is a invalid value
               std::cerr << "The value for the inode limit is not valid: " << lineList[2] <<
                  " ; Check line " << lineNumber << " of file: " << pathToFile << " Format: GID or "
                  "UID,size limit,inode limit ; Example: 2034,500M,10000 ; "
                  "Example: 2034,unlimited,reset" << std::endl;

               retVal++;
               continue;
            }

            QuotaData quota(id, quotaType);
            quota.setQuotaData(sizeLimit, inodeLimit);
            if(!QuotaData::uniqueIDAddQuotaDataToList(quota, &this->quotaLimitsList) )
               std::cerr << "The ID " << id << " in line " << lineNumber << " of file "
                  << pathToFile << " occours more then once." << std::endl;
         }
         else
         {
            std::cerr << "Check line " << lineNumber << " of file: " << pathToFile << " ; Not "
               "enough or to much values are given. Format: GID or UID,size limit,inode limit ; "
               "Example: 2034,500M,10000 ; Example: 2034,unlimited,reset" << std::endl;

            retVal++;
            continue;
         }
      }
   }

   fis.close();

   return retVal;
}

/*
 * send the quota limits to the management nodes and collect the responses
 *
 * @param mgmtNode the management node
 * @param storagePoolId a storage pool ID
 * @return error information, true on success, false if a error occurred
 *
 */
bool ModeSetQuota::uploadQuotaLimitsAndCollectResponses(Node& mgmtNode)
{
   bool retVal = true;

   std::string typeString = "user";
   if(this->cfg.cfgType == QuotaDataType_GROUP)
      typeString = "group";

   //send quota limit to management server and print the response
   // request all subranges (=> msg size limitation) from current server
   for(int messageNumber = 0; messageNumber < getMaxMessageCount(); messageNumber++)
   {
      SetQuotaMsg msg(cfg.cfgStoragePoolId);
      prepareMessage(messageNumber, &msg);

      SetQuotaRespMsg* respMsgCast;

      const auto respMsg = MessagingTk::requestResponse(mgmtNode, msg, NETMSGTYPE_SetQuotaResp);
      if (!respMsg)
      {
         std::cerr << "Failed to communicate with node: " << mgmtNode.getTypedNodeID()
            << std::endl;
         retVal = false;
         goto err_cleanup;
      }

      respMsgCast = (SetQuotaRespMsg*)respMsg.get();
      if (!respMsgCast->getValue() )
      {
         std::cerr << "Failed to set quota limits. Quota is not enabled in the management server."
            << std::endl;
         retVal = false;
         goto err_cleanup;
      }

err_cleanup:
      if(!retVal)
         break;
   }

   return retVal;
}


bool ModeSetQuota::uploadDefaultQuotaLimitsAndCollectResponses(Node& mgmtNode)
{
   bool retVal = true;

   std::string typeString = "users";
   if(this->cfg.cfgType == QuotaDataType_GROUP)
      typeString = "groups";

   SetDefaultQuotaMsg msg(cfg.cfgStoragePoolId, cfg.cfgType, cfg.cfgSizeLimit, cfg.cfgInodeLimit);

   SetDefaultQuotaRespMsg* respMsgCast;

   const auto respMsg = MessagingTk::requestResponse(mgmtNode, msg, NETMSGTYPE_SetDefaultQuotaResp);
   if (!respMsg)
   {
      std::cerr << "Failed to communicate with node: " << mgmtNode.getTypedNodeID() << std::endl;
      retVal = false;
      goto err_cleanup;
   }

   respMsgCast = (SetDefaultQuotaRespMsg*)respMsg.get();
   if (!respMsgCast->getValue() )
   {
      std::cerr << "Failed to set default quota limits. Check management server logfile of node: "
         << mgmtNode.getTypedNodeID() << std::endl;
      retVal = false;
   }

err_cleanup:

   return retVal;
}

/*
 * calculates the sublist of ID list, which fits into the payload of the msg
 *
 * @param messageNumber the sequence number of the message
 * @param msg the message to send
 */
void ModeSetQuota::prepareMessage(int messageNumber, SetQuotaMsg* msg)
{
   int counter = 0;
   int startThisMessage = (messageNumber * SETQUOTAMSG_MAX_ID_COUNT);
   int startNextMessage = ((messageNumber + 1) * SETQUOTAMSG_MAX_ID_COUNT);

   for(QuotaDataListIter iter = this->quotaLimitsList.begin(); iter != this->quotaLimitsList.end();
      iter++)
   {
      if(counter < startThisMessage)
      {
         counter++;
         continue;
      }
      else
      if(counter >= startNextMessage)
         return;

      counter++;
      msg->insertQuotaLimit(*iter);
   }
}

/*
 * calculate the number of massages which are required to upload all quota limits
 */
int ModeSetQuota::getMaxMessageCount()
{
   int retVal = 1;

   retVal =  this->quotaLimitsList.size() / SETQUOTAMSG_MAX_ID_COUNT;

   if( (this->quotaLimitsList.size() % SETQUOTAMSG_MAX_ID_COUNT) != 0)
      retVal++;

   return retVal;
}

/*
 * generates the QuotaDataList with the quota limits from the ID list
 */
void ModeSetQuota::convertIDListToQuotaDataList()
{
   for (UIntListIter iter = this->cfg.cfgIDList.begin(); iter != this->cfg.cfgIDList.end(); iter++)
   {
      QuotaData quota(*iter, this->cfg.cfgType);
      quota.setQuotaData(this->cfg.cfgSizeLimit, this->cfg.cfgInodeLimit),
      this->quotaLimitsList.push_back(quota);
   }
}

/*
 * generates the QuotaDataList with the quota limits from the ID range
 */
void ModeSetQuota::convertIDRangeToQuotaDataList()
{
   for (unsigned id = this->cfg.cfgIDRangeStart; id <= this->cfg.cfgIDRangeEnd; id++)
   {
      QuotaData quota(id, this->cfg.cfgType);
      quota.setQuotaData(this->cfg.cfgSizeLimit, this->cfg.cfgInodeLimit),
      this->quotaLimitsList.push_back(quota);
   }
}
