#include <app/App.h>
#include <common/net/message/storage/attribs/SetDirPatternMsg.h>
#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/Raid10Pattern.h>
#include <common/storage/striping/BuddyMirrorPattern.h>
#include <common/toolkit/MathTk.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeSetPattern.h"


#define MODESETPATTERN_ARG_CHUNKSIZE         "--chunksize"
#define MODESETPATTERN_ARG_NUMTARGETS        "--numtargets"
#define MODESETPATTERN_ARG_UNMOUNTEDPATH     "--unmounted"
#define MODESETPATTERN_ARG_RAID10            "--raid10"
#define MODESETPATTERN_ARG_BUDDYMIRROR       "--buddymirror"
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
      chunkSize = UnitTk::strHumanToInt64(iter->second);
      cfg->erase(iter);
   }

   if(!chunkSize)
   {
      std::cerr << "Argument missing or invalid: " << MODESETPATTERN_ARG_CHUNKSIZE << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if(chunkSize < STRIPEPATTERN_MIN_CHUNKSIZE)
   {
      std::cerr << "Chunksize too small (min: " << STRIPEPATTERN_MIN_CHUNKSIZE << "): " <<
         MODESETPATTERN_ARG_CHUNKSIZE << std::endl;
      return APPCODE_RUNTIME_ERROR;
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

   if(!numTargets)
   {
      std::cerr << "Argument missing or invalid: " << MODESETPATTERN_ARG_NUMTARGETS << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   iter = cfg->find(MODESETPATTERN_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      this->cfgUseMountedPath = false;
      cfg->erase(iter);
   }

   iter = cfg->find(MODESETPATTERN_ARG_RAID10);
   if(iter != cfg->end() )
   {
      cfgUseRaid10Pattern = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODESETPATTERN_ARG_BUDDYMIRROR);
   if(iter != cfg->end() )
   {
      cfgUseBuddyMirrorPattern = true;
      cfg->erase(iter);
   }

   if ( (cfgUseRaid10Pattern) && (cfgUseBuddyMirrorPattern) )
   {
      std::cerr << "Only one stripe pattern can be set." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if (actorUID != 0 && (cfgUseRaid10Pattern || cfgUseBuddyMirrorPattern))
   {
      std::cerr << "Only root may change the stripe pattern type." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if (cfgUseBuddyMirrorPattern
         && app->getMirrorBuddyGroupMapper()->getSize() == 0
         && cfg->count(MODESETPATTERN_ARG_FORCE) == 0)
   {
      std::cerr << "Cannot set buddy mirroring when no groups have been defined. "
         << "Override with --force." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }
   cfg->erase(MODESETPATTERN_ARG_FORCE);

   if(cfg->empty() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   this->cfgPathStr = cfg->begin()->first;
   cfg->erase(cfg->begin() );

   if(this->cfgPathStr == MODEGETENTRYINFO_ARG_READFROMSTDIN)
      cfgReadFromStdin = true;


   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   if(cfgReadFromStdin)
   { // read first path from stdin
      this->cfgPathStr.clear();
      getline(std::cin, this->cfgPathStr);

      if(this->cfgPathStr.empty() )
         return APPCODE_NO_ERROR; // no files given is not an error
   }

   // print parsed arguments
   std::cout << "New chunksize: " << chunkSize << std::endl;
   std::cout << "New number of storage targets: " << numTargets << std::endl;
   std::cout << std::endl;

   do /* loop while we get paths from stdin (or terminate if stdin reading disabled) */
   {
      std::string relPathStr;
      std::string mountRoot;
      bool setPatternRes;

      if(this->cfgUseMountedPath)
      { // make path relative to mount root

         bool pathRelativeRes = ModeHelper::makePathRelativeToMount(
            this->cfgPathStr, false, false, &mountRoot, &relPathStr);
         if(!pathRelativeRes)
            return APPCODE_RUNTIME_ERROR;
      }
      else
         relPathStr = this->cfgPathStr;

      // find owner node

      Path relPath("/" + relPathStr);

      NodeHandle ownerNode;
      EntryInfo entryInfo;

      FhgfsOpsErr findRes = MetadataTk::referenceOwner(
         &relPath, false, metaNodes, ownerNode, &entryInfo, metaBuddyGroupMapper);

      if(findRes != FhgfsOpsErr_SUCCESS)
      {
         std::cerr << "Unable to find metadata node for path: " << this->cfgPathStr << std::endl;
         std::cerr << "Error: " << FhgfsOpsErrTk::toErrString(findRes) << std::endl;
         retVal = APPCODE_RUNTIME_ERROR;

         if(!cfgReadFromStdin)
            break;

         goto finish_this_entry;
      }


      // print path info
      std::cout << "Path: " << relPathStr << std::endl;

      if (this->cfgUseMountedPath)
         std::cout << "Mount: " << mountRoot << std::endl;

      // apply the new pattern
      setPatternRes = setPattern(*ownerNode, &entryInfo, chunkSize, numTargets);
      if(!setPatternRes)
         retVal = APPCODE_RUNTIME_ERROR;

      if(!cfgReadFromStdin)
         break;


finish_this_entry:

      std::cout << std::endl;

      // read next relPath from stdin
      this->cfgPathStr.clear();
      getline(std::cin, this->cfgPathStr);

   } while(!this->cfgPathStr.empty() );

   return retVal;
}

void ModeSetPattern::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                       Path to a directory." << std::endl;
   std::cout << "                               Specify \"-\" here to read multiple paths from" << std::endl;
   std::cout << "                               stdin (separated by newline)." << std::endl;
   std::cout << "  --chunksize=<bytes>          Block size for striping (per storage target)." << std::endl;
   std::cout << "                               Suffixes 'k' (kilobytes) and 'm' (megabytes) are" << std::endl;
   std::cout << "                               allowed." << std::endl;
   std::cout << "  --numtargets=<number>        Number of stripe targets (per file)." << std::endl;
   std::cout << "                               (Number of mirror groups with \"--buddymirror\".)" << std::endl;
   std::cout << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --buddymirror                Use buddy mirror striping to mirror file contents." << std::endl;
   std::cout << "                               Each target will be mirrored on a corresponding" << std::endl;
   std::cout << "                               mirror target (its mirror buddy)." << std::endl;
   std::cout << "                               This is an enterprise feature. See end-user" << std::endl;
   std::cout << "                               license agreement for definition and usage" << std::endl;
   std::cout << "                               limitations of enterprise features." << std::endl;
   std::cout << "  --force                      Allow buddy mirror pattern to be set, even if no" << std::endl;
   std::cout << "                               groups have been defined." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode sets a new striping configuration for a certain directory. The new" << std::endl;
   std::cout << " striping configuration will only be effective for new files and new" << std::endl;
   std::cout << " subdirectories of the given directory." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set directory pattern to 1 megabyte chunks and 4 targets per file" << std::endl;
   std::cout << "  $ beegfs-ctl --setpattern --chunksize=1m --numtargets=4 /mnt/beegfs/mydir" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Apply new pattern to all subdirectories of /mnt/beegfs/mydir" << std::endl;
   std::cout << "  $ find /mnt/beegfs/mydir -type d | \\" << std::endl;
   std::cout << "     beegfs-ctl --setpattern --chunksize=1m --numtargets=4 -" << std::endl;
}

bool ModeSetPattern::setPattern(Node& ownerNode, EntryInfo* entryInfo, unsigned chunkSize,
   unsigned numTargets)
{
   bool retVal = false;

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   SetDirPatternRespMsg* respMsgCast;

   FhgfsOpsErr setPatternRes;
   UInt16Vector stripeTargetIDs;
   UInt16Vector mirrorTargetIDs;
   StripePattern* pattern;

   if (cfgUseRaid10Pattern)
      pattern = new Raid10Pattern(chunkSize, stripeTargetIDs, mirrorTargetIDs, numTargets);
   else
   if (cfgUseBuddyMirrorPattern)
      pattern = new BuddyMirrorPattern(chunkSize, stripeTargetIDs, numTargets);
   else
      pattern = new Raid0Pattern(chunkSize, stripeTargetIDs, numTargets);

   // todo: switch to ioctl, so that users can set patterns without root privileges

   SetDirPatternMsg setPatternMsg(entryInfo, pattern);

   if (actorUID != 0)
      setPatternMsg.setUID(actorUID);

   // request/response
   commRes = MessagingTk::requestResponse(
      ownerNode, &setPatternMsg, NETMSGTYPE_SetDirPatternResp, &respBuf, &respMsg);
   if(!commRes)
   {
      std::cerr << "Communication with server failed: " << ownerNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (SetDirPatternRespMsg*)respMsg;

   setPatternRes = (FhgfsOpsErr)respMsgCast->getValue();
   if(setPatternRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Node encountered an error: " << FhgfsOpsErrTk::toErrString(setPatternRes) <<
         std::endl;
      goto err_cleanup;
   }

   retVal = true;

err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   delete(pattern);

   return retVal;
}
