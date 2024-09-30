#include <app/App.h>
#include <common/net/message/storage/attribs/SetDirPatternMsg.h>
#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/BuddyMirrorPattern.h>
#include <common/toolkit/MathTk.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeSetPattern.h"
#include <cstdint>

#define MODESETPATTERN_ARG_CHUNKSIZE         "--chunksize"
#define MODESETPATTERN_ARG_NUMTARGETS        "--numtargets"
#define MODESETPATTERN_ARG_STORAGEPOOL       "--storagepoolid"
#define MODESETPATTERN_ARG_UNMOUNTEDPATH     "--unmounted"
#define MODESETPATTERN_ARG_PATTERN           "--pattern"
#define MODESETPATTERN_ARG_FORCE             "--force"
#define MODEGETENTRYINFO_ARG_READFROMSTDIN   "-"


int ModeSetPattern::execute()
{
   int retVal = APPCODE_NO_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   StringMapIter iter;

   actorUID = geteuid();

   // check arguments

   unsigned chunkSize = 0;
   iter = cfg->find(MODESETPATTERN_ARG_CHUNKSIZE);
   if(iter != cfg->end() )
   {
      int64_t rawChunksize = 0;
      rawChunksize = UnitTk::strHumanToInt64(iter->second);
   if (rawChunksize < STRIPEPATTERN_MIN_CHUNKSIZE || rawChunksize > UINT32_MAX) {
      std::cerr << "Chunksize " << MODESETPATTERN_ARG_CHUNKSIZE << " out of bounds, must be between " 
         << STRIPEPATTERN_MIN_CHUNKSIZE << " and " << UINT32_MAX << std::endl;
      return APPCODE_RUNTIME_ERROR;
    }
      chunkSize = static_cast<unsigned>(rawChunksize);
      cfg->erase(iter);
   }

   if(!MathTk::isPowerOfTwo(chunkSize) )
   {
      std::cerr << "Chunksize must be a power of two: " << MODESETPATTERN_ARG_CHUNKSIZE <<
         std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   unsigned numTargets = 0;
   iter = cfg->find(MODESETPATTERN_ARG_NUMTARGETS);
   if(iter != cfg->end() )
   {
      numTargets = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   bool forceMode = false;
   iter = cfg->find(MODESETPATTERN_ARG_FORCE);
   if (iter != cfg->end())
   {
      forceMode = true;
      cfg->erase(iter);
   }

   StoragePoolId storagePoolId(StoragePoolStore::INVALID_POOL_ID);
   iter = cfg->find(MODESETPATTERN_ARG_STORAGEPOOL);
   if(iter != cfg->end() )
   {
      if (StringTk::isNumeric(iter->second))
      {
         storagePoolId.fromStr(iter->second);
      }
      else if (iter->second == "default")
      {
         storagePoolId = StoragePoolStore::DEFAULT_POOL_ID;
      }
      else
      {
         std::cerr << "Storage pool must be a positive numeric value less than "
                   << std::numeric_limits<StoragePoolId>::max() << " or default.";

         return APPCODE_RUNTIME_ERROR;
      }

      const auto poolStore = Program::getApp()->getStoragePoolStore();

      if (!poolStore)
         throw std::runtime_error("Unable to access StoragePools");

      const auto pool = poolStore->getPool(storagePoolId);

      // check if pool exists
      if (!pool)
      {
         std::cerr << "Storage pool with ID " << storagePoolId << " does not exist." << std::endl;

         return APPCODE_RUNTIME_ERROR;
      }

      // make sure pool has targets
      if (pool->getTargets().empty() && !forceMode)
      {
         std::cerr << "The desired Storage Pool (" << storagePoolId.val()
                   << ") does not contain any targets. Use --force to override." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      cfg->erase(iter);
   }


   bool useMountedPath = true;
   iter = cfg->find(MODESETPATTERN_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      useMountedPath = false;
      cfg->erase(iter);
   }


   StripePatternType patternType = StripePatternType_Invalid;
   iter = cfg->find(MODESETPATTERN_ARG_PATTERN);
   if (iter != cfg->end())
   {
      if (iter->second == "raid0")
      {
         patternType = StripePatternType_Raid0;
      }
      else if (iter->second == "buddymirror")
      {
         patternType = StripePatternType_BuddyMirror;
      }
      else
      {
         std::cerr << MODESETPATTERN_ARG_PATTERN << " must be raid0 or buddymirror."
               << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
      cfg->erase(iter);
   }


   if (actorUID != 0 && patternType != StripePatternType_Invalid)
   {
      std::cerr << "Only root may change the stripe pattern type." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if (patternType == StripePatternType_BuddyMirror
         && app->getMirrorBuddyGroupMapper()->getSize() == 0
         && !forceMode)
   {
      std::cerr << "Cannot set buddy mirroring when no groups have been defined. "
         << "Override with --force." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if(cfg->empty() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   std::string pathStr = cfg->begin()->first;
   cfg->erase(cfg->begin() );


   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   bool readFromStdin = (pathStr == MODEGETENTRYINFO_ARG_READFROMSTDIN);

   if (readFromStdin)
   { // read first path from stdin
      pathStr.clear();
      getline(std::cin, pathStr);

      if (pathStr.empty() )
         return APPCODE_NO_ERROR; // no files given is not an error
   }

   // print parsed arguments
   if (chunkSize != 0)
      std::cout << "New chunksize: " << chunkSize << std::endl;
   if (numTargets != 0)
      std::cout << "New number of storage targets: " << numTargets << std::endl;
   if (storagePoolId != StoragePoolStore::INVALID_POOL_ID)
      std::cout << "New storage pool ID: " << storagePoolId << std::endl;
   std::cout << std::endl;

   do /* loop while we get paths from stdin (or terminate if stdin reading disabled) */
   {
      std::string mountRoot;
      bool setPatternRes;

      // find owner node

      Path path(pathStr);

      NodeHandle ownerNode;
      EntryInfo entryInfo;

      if(!ModeHelper::getEntryAndOwnerFromPath(path, useMountedPath,
            *metaNodes, app->getMetaRoot(), *metaBuddyGroupMapper,
            entryInfo, ownerNode))
      {
         retVal = APPCODE_RUNTIME_ERROR;
         if (!readFromStdin)
            break;

         goto finish_this_entry;
      }

      // apply the new pattern
      setPatternRes = setPattern(*ownerNode, &entryInfo, chunkSize, numTargets, storagePoolId,
            patternType);
      if(!setPatternRes)
         retVal = APPCODE_RUNTIME_ERROR;

      if (!readFromStdin)
         break;


finish_this_entry:

      std::cout << std::endl;

      // read next relPath from stdin
      pathStr.clear();
      getline(std::cin, pathStr);

   } while (!pathStr.empty() );

   return retVal;
}

void ModeSetPattern::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                       Path to a directory." << std::endl;
   std::cout << "                               Specify \"-\" here to read multiple paths from" << std::endl;
   std::cout << "                               stdin (separated by newline)." << std::endl;
   std::cout << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --storagepoolid=<poolID>     Use poolID as storage pool for new files in this" << std::endl;
   std::cout << "                               directory. Can be set to \"default\" to use " << std::endl;
   std::cout << "                               the default pool." << std::endl;
   std::cout << "  --chunksize=<bytes>          Block size for striping (per storage target)." << std::endl;
   std::cout << "                               Suffixes 'k' (kilobytes) and 'm' (megabytes) are" << std::endl;
   std::cout << "                               allowed." << std::endl;
   std::cout << "  --pattern=<pattern>          Set the stripe pattern type to use. Can be" << std::endl;
   std::cout << "                               raid0 (default) or buddymirror. Buddy mirroring" << std::endl;
   std::cout << "                               mirrors file contents across targets." << std::endl;
   std::cout << "                               Each target will be mirrored on a corresponding" << std::endl;
   std::cout << "                               mirror target (its mirror buddy)." << std::endl;
   std::cout << "                               This is an enterprise feature. See end-user" << std::endl;
   std::cout << "                               license agreement for definition and usage" << std::endl;
   std::cout << "                               limitations of enterprise features." << std::endl;
   std::cout << "  --numtargets=<number>        Number of stripe targets (per file)." << std::endl;
   std::cout << "                               If the stripe pattern is set to 'buddymirror'," << std::endl;
   std::cout << "                               this is the number of mirror groups." << std::endl;
   std::cout << "  --force                      Allow buddy mirror pattern to be set, even if no" << std::endl;
   std::cout << "                               groups have been defined." << std::endl;
   std::cout << "  --unmounted                  If this is specified then the given path is" << std::endl;
   std::cout << "                               relative to the root directory of a possibly" << std::endl;
   std::cout << "                               unmounted BeeGFS. (Symlinks will not be resolved" << std::endl;
   std::cout << "                               in this case.)" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode sets a new striping configuration for a certain directory. The new" << std::endl;
   std::cout << " striping configuration will only be effective for new files and new" << std::endl;
   std::cout << " subdirectories of the given directory." << std::endl;
   std::cout << std::endl;
   std::cout << " This mode can be used by non-root users only if the administrator" << std::endl;
   std::cout << " enabled `sysAllowUserSetPattern` in the metadata server config." << std::endl;
   std::cout << " This enables normal users to change the number of stripe targets" << std::endl;
   std::cout << " (--numtargets) and the chunk size (--chunksize) for files that they" << std::endl;
   std::cout << " own themselves. All other parameters can only be changed by root." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set directory pattern to 1 megabyte chunks and 4 targets per file" << std::endl;
   std::cout << "  # beegfs-ctl --setpattern --chunksize=1m --numtargets=4 /mnt/beegfs/mydir" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Apply new pattern to all subdirectories of /mnt/beegfs/mydir" << std::endl;
   std::cout << "  # find /mnt/beegfs/mydir -type d | \\" << std::endl;
   std::cout << "     beegfs-ctl --setpattern --chunksize=1m --numtargets=4 -" << std::endl;
}

bool ModeSetPattern::setPattern(Node& ownerNode, EntryInfo* entryInfo, unsigned chunkSize,
   unsigned numTargets, StoragePoolId storagePoolId, StripePatternType patternType)
{
   // retrieve current pattern settings

   GetEntryInfoMsg getInfoMsg(entryInfo);

   auto infoRespMsg = MessagingTk::requestResponse(ownerNode, getInfoMsg,
         NETMSGTYPE_GetEntryInfoResp);

   if (!infoRespMsg)
   {
      std::cerr << "Communication with server failed: " << ownerNode.getNodeIDWithTypeStr() <<
         std::endl;
      return false;
   }

   const auto infoRespMsgCast = static_cast<GetEntryInfoRespMsg*>(infoRespMsg.get());

   FhgfsOpsErr getInfoRes = infoRespMsgCast->getResult();
   if (getInfoRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error: " << ownerNode.getNodeIDWithTypeStr() << "; " <<
         "Error: " << getInfoRes << std::endl;
      return false;
   }

   const StripePattern& existingPattern = infoRespMsgCast->getPattern();

   // if setting not set, use current

   if (chunkSize == 0)
      chunkSize = existingPattern.getChunkSize();

   if (numTargets == 0)
      numTargets = existingPattern.getDefaultNumTargets();

   if (storagePoolId == StoragePoolStore::INVALID_POOL_ID)
      storagePoolId = existingPattern.getStoragePoolId();

   if (patternType == StripePatternType_Invalid)
      patternType = existingPattern.getPatternType();



   FhgfsOpsErr setPatternRes;
   UInt16Vector stripeTargetIDs;
   UInt16Vector mirrorTargetIDs;
   std::unique_ptr<StripePattern> pattern;

   if (patternType == StripePatternType_BuddyMirror)
   {
      pattern.reset(new BuddyMirrorPattern(chunkSize, stripeTargetIDs, numTargets, storagePoolId));
   }
   else
   {
      pattern.reset(new Raid0Pattern(chunkSize, stripeTargetIDs, numTargets, storagePoolId));
   }

   // todo: switch to ioctl, so that users can set patterns without root privileges

   SetDirPatternMsg setPatternMsg(entryInfo, pattern.get());

   if (actorUID != 0)
      setPatternMsg.setUID(actorUID);

   const auto respMsg = MessagingTk::requestResponse(ownerNode, setPatternMsg,
         NETMSGTYPE_SetDirPatternResp);
   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << ownerNode.getNodeIDWithTypeStr() <<
         std::endl;
      return false;
   }

   SetDirPatternRespMsg* respMsgCast = (SetDirPatternRespMsg*)respMsg.get();

   setPatternRes = (FhgfsOpsErr)respMsgCast->getValue();
   if(setPatternRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Node encountered an error: " << setPatternRes << std::endl;
      return false;
   }

   return true;
}
