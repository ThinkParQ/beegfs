#include <app/App.h>
#include <common/net/message/storage/creating/MkFileWithPatternMsg.h>
#include <common/net/message/storage/creating/MkFileWithPatternRespMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/creating/MkFileMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/BuddyMirrorPattern.h>
#include <common/storage/striping/StripePattern.h>
#include <common/toolkit/MathTk.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeCreateFile.h"

#include <functional>

#define MODECREATEFILE_ARG_CHUNKSIZE      "--chunksize"
#define MODECREATEFILE_ARG_NUMTARGETS     "--numtargets"
#define MODECREATEFILE_ARG_TARGETS        "--targets"
#define MODECREATEFILE_ARG_STORAGEPOOL    "--storagepoolid"
#define MODECREATEFILE_ARG_PERMISSIONS    "--access"
#define MODECREATEFILE_ARG_USERID         "--uid"
#define MODECREATEFILE_ARG_GROUPID        "--gid"
#define MODECREATEFILE_ARG_UNMOUNTEDPATH  "--unmounted"
#define MODECREATEFILE_ARG_FORCE          "--force"
#define MODECREATEFILE_ARG_PATTERN        "--pattern"


int ModeCreateFile::execute()
{
   App* app = Program::getApp();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // PARSE CONFIG ARGUMENTS

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   StripePatternType patternType = StripePatternType_Invalid;
   iter = cfg->find(MODECREATEFILE_ARG_PATTERN);
   if(iter != cfg->end() )
   {
      if(iter->second == "raid0")
      {
         patternType = StripePatternType_Raid0;
      }
      else if(iter->second == "buddymirror")
      {
         patternType = StripePatternType_BuddyMirror;
      }
      else
      {
         std::cerr << MODECREATEFILE_ARG_PATTERN << " must be raid0 or buddymirror."
               << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
      cfg->erase(iter);
   }

   unsigned chunkSize = 0;
   iter = cfg->find(MODECREATEFILE_ARG_CHUNKSIZE);
   if (iter != cfg->end())
   {
      chunkSize = UnitTk::strHumanToInt64(iter->second);
      if(!MathTk::isPowerOfTwo(chunkSize))
      {
         std::cerr << "Invalid value for " << MODECREATEFILE_ARG_CHUNKSIZE;
         std::cerr << ": Must be a power of two." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
      if(chunkSize < STRIPEPATTERN_MIN_CHUNKSIZE)
      {
         std::cerr << "Invalid value for " << MODECREATEFILE_ARG_CHUNKSIZE;
         std::cerr << ": Minimum chunk size is " << STRIPEPATTERN_MIN_CHUNKSIZE << "." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
      cfg->erase(iter);
   }

   unsigned numTargets = 0;
   iter = cfg->find(MODECREATEFILE_ARG_NUMTARGETS);
   if (iter != cfg->end())
   {
      numTargets = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   int mode = 0644;
   iter = cfg->find(MODECREATEFILE_ARG_PERMISSIONS);
   if(iter != cfg->end() )
   {
      mode = StringTk::strOctalToUInt(iter->second);
      mode = mode & 07777; // trim invalid flags from mode
      cfg->erase(iter);
   }
   mode |= S_IFREG; // make sure mode contains the "file" flag

   unsigned userID = 0;
   iter = cfg->find(MODECREATEFILE_ARG_USERID);
   if(iter != cfg->end() )
   {
      userID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   unsigned groupID = 0;
   iter = cfg->find(MODECREATEFILE_ARG_GROUPID);
   if(iter != cfg->end() )
   {
      groupID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   UInt16Vector targets;
   StringList targetsUntrimmed;
   iter = cfg->find(MODECREATEFILE_ARG_TARGETS);
   if(iter != cfg->end() )
   {
      StringTk::explode(iter->second, ',', &targetsUntrimmed);
      cfg->erase(iter);

      // trim nodes (copy to settings list)
      for (StringListIter targetsIter = targetsUntrimmed.begin();
          targetsIter != targetsUntrimmed.end();
          targetsIter++)
      {
         std::string currentTrimmedTarget = StringTk::trim(*targetsIter);
         uint16_t currentTargetID = StringTk::strToUInt(currentTrimmedTarget);

         if (std::find(targets.begin(), targets.end(), currentTargetID) != targets.end())
         {
            std::cerr << "Each storage target may be used only once in a stripe pattern."
                  << std::endl;
            return APPCODE_RUNTIME_ERROR;
         }
         targets.push_back(currentTargetID);
      }
   }

   bool useMountedPath = true;
   iter = cfg->find(MODECREATEFILE_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      useMountedPath = false;
      cfg->erase(iter);
   }

   StoragePoolId storagePoolId(StoragePoolStore::INVALID_POOL_ID);
   bool storagePoolSet = false;
   iter = cfg->find(MODECREATEFILE_ARG_STORAGEPOOL);
   if (iter != cfg->end())
   {
      if (StringTk::isNumeric(iter->second))
      {
         storagePoolId.fromStr(iter->second);
      }
      else if (iter->second == "default")
      {
         storagePoolId = StoragePoolStore::DEFAULT_POOL_ID;
      }
      else if (iter->second != "inherit")
      {
         std::cerr << MODECREATEFILE_ARG_STORAGEPOOL << " must be a numeric ID "
               << ", default or inherit." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      storagePoolSet = true;

      cfg->erase(iter);
   }

   bool useForce = false;
   iter = cfg->find(MODECREATEFILE_ARG_FORCE);
   if (iter != cfg->end())
   {
      useForce = true;
      cfg->erase(iter);
   }

   Path path;
   iter = cfg->begin();
   if(iter == cfg->end() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }
   else
   {
      path = iter->first;

      if (path.empty())
      {
         std::cerr << "Invalid path specified." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }

   if (storagePoolSet && storagePoolId != StoragePoolStore::INVALID_POOL_ID)
   {
      StoragePoolPtr storagePool =
            Program::getApp()->getStoragePoolStore()->getPool(storagePoolId);
      if (!storagePool)
      {
         std::cerr << "Given storage pool " << storagePoolId.val()
               << " doesn't exist." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }

   // Find owner node

   NodeHandle ownerNode;
   EntryInfo ownerInfo;
   {
      auto dirname = path.dirname();
      if (!ModeHelper::getEntryAndOwnerFromPath(dirname, useMountedPath,
            *app->getMetaNodes(), app->getMetaRoot(),*app->getMetaMirrorBuddyGroupMapper(),
            ownerInfo, ownerNode))
      {
         return APPCODE_RUNTIME_ERROR;
      }
   }

   // retrieve pattern from parent folder

   GetEntryInfoMsg getInfoMsg(&ownerInfo);

   const auto infoRespMsg = MessagingTk::requestResponse(*ownerNode, getInfoMsg,
         NETMSGTYPE_GetEntryInfoResp);

   if (!infoRespMsg)
   {
      std::cerr << "Communication with server failed: " << ownerNode->getNodeIDWithTypeStr() <<
         std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   const auto infoRespMsgCast = static_cast<GetEntryInfoRespMsg*>(infoRespMsg.get());

   FhgfsOpsErr getInfoRes = infoRespMsgCast->getResult();
   if (getInfoRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error: " << ownerNode->getNodeIDWithTypeStr() << "; " <<
         "Error: " << getInfoRes << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   const StripePattern& parentPattern = infoRespMsgCast->getPattern();

   // if pattern type is not set, inherit from parent.
   // rest of the arguments are inherited below, after the checks
   if (patternType == StripePatternType_Invalid)
      patternType = parentPattern.getPatternType();


   if ((!storagePoolSet && targets.empty()) || (storagePoolSet && !targets.empty()))
   {
      std::cerr << "Exactly one of " << MODECREATEFILE_ARG_STORAGEPOOL
            << " and " << MODECREATEFILE_ARG_TARGETS << " must be set." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if (!targets.empty() && (targets.size() < numTargets))
   {
      std::cerr << "Not enough targets specified for the given \"numtargets\" setting." <<
         std::endl;
      return APPCODE_RUNTIME_ERROR;
   }


   // BuddyGroupPattern related checks
   if (patternType == StripePatternType_BuddyMirror)
   {
      if (!targets.empty() && (targets.size() < BuddyMirrorPattern::getMinNumTargetsStatic()))
      {
         std::cerr << "Not enough buddy groups available for the stripe pattern."
               << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      // check given buddy groups for existence
      if (!targets.empty())
      {
         for (auto target = targets.begin(); target != targets.end(); ++target)
         {
            if (!Program::getApp()->getMirrorBuddyGroupMapper()->getPrimaryTargetID(*target))
            {
               std::cerr << "Given buddy group " << *target << " doesn't exist." << std::endl;
               return APPCODE_RUNTIME_ERROR;
            }
         }
      }
   }
   // Raid0Pattern related checks
   else
   {
      if (!targets.empty() && (targets.size() < Raid0Pattern::getMinNumTargetsStatic()))
      {
         std::cerr << "Not enough storage targets available for the stripe pattern." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      // check given targets for existence
      if (!targets.empty())
      {
         for (auto target = targets.begin(); target != targets.end(); ++target)
         {
            if (!Program::getApp()->getTargetMapper()->targetExists(*target))
            {
               std::cerr << "Given target " << *target << " doesn't exist." << std::endl;
               return APPCODE_RUNTIME_ERROR;
            }
         }
      }
   }


   // If not given a force option, test if all given targets are within the same storage pool
   if (!targets.empty() && !useForce)
   {
      StoragePoolPtrVec storagePools =
            Program::getApp()->getStoragePoolStore()->getPoolsAsVec();

      StoragePoolId firstPoolID;

      for (auto t = targets.begin(); t != targets.end(); ++t)
      {
         StoragePoolPtrVecCIter pool;
         if (patternType == StripePatternType_BuddyMirror)
         {
            pool = std::find_if(storagePools.begin(), storagePools.end(),
                  [&] (const auto& p) { return p->hasBuddyGroup(*t); });
         }
         else
         {
            pool = std::find_if(storagePools.begin(), storagePools.end(),
                  [&] (const auto& p) { return p->hasTarget(*t); });
         }

         if (pool == storagePools.end())
         {
            std::cerr << "Given target / buddy group " << *t
                  << " can't be matched to any storage pool." << std::endl;
            return APPCODE_RUNTIME_ERROR;
         }

         if (firstPoolID.val() == 0)
         {
            firstPoolID = (*pool)->getId();
         }
         else if ((*pool)->getId() != firstPoolID)
         {
            std::cerr << "Given targets / buddy groups aren't in the same storage pool."
                  << " If this is intended, use --force to overwrite." << std::endl;
            return APPCODE_RUNTIME_ERROR;
         }
      }
   }

   // if some settings are not set, inherit them from parent

   if (chunkSize == 0)
      chunkSize = parentPattern.getChunkSize();

   if (numTargets == 0)
      numTargets = parentPattern.getDefaultNumTargets();

   if (storagePoolId == StoragePoolStore::INVALID_POOL_ID)
      storagePoolId = parentPattern.getStoragePoolId();



   // create pattern, type depending on setting

   std::unique_ptr<StripePattern> pattern;

   if(patternType == StripePatternType_BuddyMirror)
   {
      pattern.reset(new BuddyMirrorPattern(chunkSize, targets, numTargets, storagePoolId));
   }
   else
   {
      pattern.reset(new Raid0Pattern(chunkSize, targets, numTargets, storagePoolId));
   }


   // Send data to Meta

   FhgfsOpsErr mkFileRes;
   std::string newFileName = path.back();

   MkFileWithPatternMsg msg(&ownerInfo, newFileName, userID, groupID, mode, 0000, pattern.get());

   // request/response
   const auto respMsg = MessagingTk::requestResponse(
      *ownerNode, msg, NETMSGTYPE_MkFileWithPatternResp);

   if(!respMsg)
   {
      std::cerr << "Communication with server failed: " << ownerNode->getNodeIDWithTypeStr() <<
            std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   MkFileWithPatternRespMsg* respMsgCast = (MkFileWithPatternRespMsg*)respMsg.get();

   mkFileRes = (FhgfsOpsErr)respMsgCast->getResult();
   if(mkFileRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Node encountered an error: " << mkFileRes << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   std::cout << "Operation succeeded." << std::endl;

   return APPCODE_NO_ERROR;
}

void ModeCreateFile::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << "  Mandatory:" << std::endl;
   std::cout << "   <path>                   Path of the new file." << std::endl;
   std::cout << "   --storagepoolid=<poolID> The storage pool the new file should be in." << std::endl;
   std::cout << "                            Corresponding targets are chosen automatically." << std::endl;
   std::cout << "                            Can be set to \"inherit\" to inherit from the parent " << std::endl;
   std::cout << "                            directory or \"default\" to use the default pool." << std::endl;
   std::cout << "   OR                       " << std::endl;
   std::cout << "   --targets=<targetlist>   Comma-separated list of targetIDs to use for the new" << std::endl;
   std::cout << "                            file. The list may be longer than the given" << std::endl;
   std::cout << "                            numtargets value. If the given targets are not in" << std::endl;
   std::cout << "                            the same storage pool, the creation fails. The check" << std::endl;
   std::cout << "                            can be omitted with the --force option." << std::endl;
   std::cout << "                            When using the buddy mirror pattern this argument expects" << std::endl;
   std::cout << "                            buddy group IDs instead of target IDs." << std::endl;
   std::cout << "  Optional:" << std::endl;
   std::cout << "   --pattern=<pattern>      Stripe pattern type to use. Can be raid0 or " << std::endl;
   std::cout << "                            buddymirror. If not given the pattern type is" << std::endl;
   std::cout << "                            inherited from the parent folder. Buddy mirroring" << std::endl;
   std::cout << "                            mirrors file contents across targets." << std::endl;
   std::cout << "                            Each target will be mirrored on a corresponding" << std::endl;
   std::cout << "                            mirror target (its mirror buddy)." << std::endl;
   std::cout << "                            Buddy mirroring is an enterprise feature. See end-user" << std::endl;
   std::cout << "                            license agreement for definition and usage" << std::endl;
   std::cout << "                            limitations of enterprise features." << std::endl;
   std::cout << "   --chunksize=<bytes>      Block size for striping (per node)." << std::endl;
   std::cout << "                            Suffixes 'k' (kilobytes) and 'm' (megabytes) are" << std::endl;
   std::cout << "                            allowed." << std::endl;
   std::cout << "   --numtargets=<number>    Number of stripe nodes (per file)." << std::endl;
   std::cout << "   --access=<mode>          The octal access permissions value for user, group" << std::endl;
   std::cout << "                            and others. (Default: 0644)" << std::endl;
   std::cout << "   --uid=<userid_num>       User ID of the file owner." << std::endl;
   std::cout << "   --gid=<groupid_num>      Group ID of the file." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode creates a new file." << std::endl;
   std::cout << std::endl;
   std::cout << "Example: Create file with 1 megabyte chunks on 4 storage targets" << std::endl;
   std::cout << "in default storage pool:" << std::endl;
   std::cout << "  # beegfs-ctl --createfile --chunksize=1m \\" << std::endl;
   std::cout << "    --numtargets=4 --storagepoolid=default /mnt/beegfs/myfile" << std::endl;
}
