#include <app/App.h>
#include <common/net/message/storage/creating/MkFileWithPatternMsg.h>
#include <common/net/message/storage/creating/MkFileWithPatternRespMsg.h>
#include <common/net/message/storage/creating/MkFileMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeCreateFile.h"

#define MODECREATEFILE_ARG_CHUNKSIZE      "--chunksize"
#define MODECREATEFILE_ARG_NUMTARGETS     "--numtargets"
#define MODECREATEFILE_ARG_TARGETS        "--targets"
#define MODECREATEFILE_ARG_PERMISSIONS    "--access"
#define MODECREATEFILE_ARG_USERID         "--uid"
#define MODECREATEFILE_ARG_GROUPID        "--gid"
#define MODECREATEFILE_ARG_UNMOUNTEDPATH  "--unmounted"


int ModeCreateFile::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   FileSettings settings;

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   if(!initFileSettings(&settings) )
      return APPCODE_RUNTIME_ERROR;

   // find owner node
   NodeHandle ownerNode;
   EntryInfo entryInfo;

   FhgfsOpsErr findRes = MetadataTk::referenceOwner(settings.path, true, metaNodes, ownerNode,
      &entryInfo, metaBuddyGroupMapper);
   if(findRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Unable to find metadata node for path: " << settings.path->str() <<
         std::endl;
      std::cerr << "Error: " << FhgfsOpsErrTk::toErrString(findRes) << std::endl;
      retVal = APPCODE_RUNTIME_ERROR;
      goto cleanup_settings;
   }

   // create the file, FIXME: Switch to ioctl
   if(communicateWithPattern(*ownerNode, &entryInfo, &settings) )
   {
      std::cout << "Operation succeeded." << std::endl;

      retVal = APPCODE_NO_ERROR;
   }

cleanup_settings:
   freeFileSettings(&settings);

   return retVal;
}

void ModeCreateFile::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << "  Mandatory:" << std::endl;
   std::cout << "   <path>                 Path of the new file." << std::endl;
   std::cout << "   --chunksize=<bytes>    Block size for striping (per node)." << std::endl;
   std::cout << "                          Suffixes 'k' (kilobytes) and 'm' (megabytes) are" << std::endl;
   std::cout << "                          allowed." << std::endl;
   std::cout << "   --numtargets=<number>  Number of stripe nodes (per file)." << std::endl;
   std::cout << "  Optional:" << std::endl;
   std::cout << "   --targets=<targetlist> Comma-separated list of targetIDs to use for the new" << std::endl;
   std::cout << "                          file. (The list may be longer than the given" << std::endl;
   std::cout << "                          numtargets value.)" << std::endl;
   std::cout << "   --access=<mode>        The octal access permissions value for user, group" << std::endl;
   std::cout << "                          and others. (Default: 0644)" << std::endl;
   std::cout << "   --uid=<userid_num>     User ID of the file owner." << std::endl;
   std::cout << "   --gid=<groupid_num>    Group ID of the file." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode creates a new file." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Create file with 1 megabyte chunks on 4 storage targets" << std::endl;
   std::cout << "  $ beegfs-ctl --createfile --chunksize=1m --numtargets=4 /mnt/beegfs/myfile" << std::endl;
}

/**
 * Note: Remember to call freeFileSettings() when you're done.
 */
bool ModeCreateFile::initFileSettings(FileSettings* settings)
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   settings->preferredTargets = NULL;
   settings->pattern = NULL;
   settings->path = NULL;

   StringMapIter iter;

   // parse and validate command line args

   unsigned chunkSize;
   iter = cfg->find(MODECREATEFILE_ARG_CHUNKSIZE);
   if(iter == cfg->end() )
   {
      std::cerr << "Argument missing: " << MODECREATEFILE_ARG_CHUNKSIZE << std::endl;
      return false;
   }
   else
   {
      chunkSize = UnitTk::strHumanToInt64(iter->second);
      cfg->erase(iter);
   }

   unsigned numTargets;
   iter = cfg->find(MODECREATEFILE_ARG_NUMTARGETS);
   if(iter == cfg->end() )
   {
      std::cerr << "Argument missing: " << MODECREATEFILE_ARG_NUMTARGETS << std::endl;
      return false;
   }
   else
   {
      numTargets = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   settings->mode = 0644;
   iter = cfg->find(MODECREATEFILE_ARG_PERMISSIONS);
   if(iter != cfg->end() )
   {
      settings->mode = StringTk::strOctalToUInt(iter->second);
      settings->mode = settings->mode & 0777; // trim invalid flags from mode
      cfg->erase(iter);
   }

   settings->mode |= S_IFREG; // make sure mode contains the "file" flag

   settings->userID = 0;
   iter = cfg->find(MODECREATEFILE_ARG_USERID);
   if(iter != cfg->end() )
   {
      settings->userID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   settings->groupID = 0;
   iter = cfg->find(MODECREATEFILE_ARG_GROUPID);
   if(iter != cfg->end() )
   {
      settings->groupID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   settings->preferredTargets = new UInt16List;
   StringList preferredTargetsUntrimmed;
   iter = cfg->find(MODECREATEFILE_ARG_TARGETS);
   if(iter != cfg->end() )
   {
      StringTk::explode(iter->second, ',', &preferredTargetsUntrimmed);
      cfg->erase(iter);

      // trim nodes (copy to settings list)
      for(StringListIter targetsIter = preferredTargetsUntrimmed.begin();
          targetsIter != preferredTargetsUntrimmed.end();
          targetsIter++)
      {
         std::string currentTrimmedTarget = StringTk::trim(*targetsIter);
         uint16_t currentTargetID = StringTk::strToUInt(currentTrimmedTarget);

         settings->preferredTargets->push_back(currentTargetID);
      }
   }

   bool useMountedPath = true;
   iter = cfg->find(MODECREATEFILE_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      useMountedPath = false;
      cfg->erase(iter);
   }

   iter = cfg->begin();
   if(iter == cfg->end() )
   {
      std::cerr << "No path specified." << std::endl;
      return false;
   }
   else
   {
      std::string pathStr = iter->first;

      if(useMountedPath)
      { // make path relative to mount root
         std::string mountRoot;
         std::string relativePath;

         bool pathRelativeRes = ModeHelper::makePathRelativeToMount(
            pathStr, true, false, &mountRoot, &relativePath);
         if(!pathRelativeRes)
            return false;

         pathStr = relativePath;

         std::cout << "Mount: '" << mountRoot << "'; Path: '" << relativePath << "'" << std::endl;
      }

      settings->path = new Path("/" + pathStr);

      if(settings->path->empty() )
      {
         std::cerr << "Invalid path specified." << std::endl;
         return false;
      }

   }

   // create pattern

   // note: empty stripeNodes is not an error. it just means the server will choose the targets.

   UInt16Vector stripeTargets(settings->preferredTargets->begin(),
      settings->preferredTargets->end() );

   if(!stripeTargets.empty() && (stripeTargets.size() < Raid0Pattern::getMinNumTargetsStatic() ) )
   { // not enough nodes for the desired pattern => improvise (use any other pattern)
      std::cerr << "Not enough storage targets available for the stripe pattern." << std::endl;
      return false;
   }

   if(!stripeTargets.empty() && (stripeTargets.size() < numTargets) )
   { // not enough nodes for the desired pattern => improvise (use any other pattern)
      std::cerr << "Not enough storage targets specified for the given \"numtargets\" setting." <<
         std::endl;
      return false;
   }

   settings->pattern = new Raid0Pattern(chunkSize, stripeTargets, numTargets);


   return true;
}

void ModeCreateFile::freeFileSettings(FileSettings* settings)
{
   SAFE_DELETE(settings->preferredTargets);
   SAFE_DELETE(settings->pattern);
   SAFE_DELETE(settings->path);
}

std::string ModeCreateFile::generateServerPath(FileSettings* settings, std::string entryID)
{
   return entryID + "/" + settings->path->back();
}

/**
 * Sends a pre-created stripe pattern.
 */
bool ModeCreateFile::communicateWithPattern(Node& ownerNode, EntryInfo* parentInfo,
   FileSettings* settings)
{
   bool retVal = false;

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   MkFileWithPatternRespMsg* respMsgCast;

   FhgfsOpsErr mkFileRes;
   std::string newFileName = settings->path->back();

   MkFileWithPatternMsg msg(parentInfo, newFileName, settings->userID, settings->groupID,
      settings->mode, 0000, settings->pattern);

   // request/response
   commRes = MessagingTk::requestResponse(
      ownerNode, &msg, NETMSGTYPE_MkFileWithPatternResp, &respBuf, &respMsg);
   if(!commRes)
   {
      std::cerr << "Communication with server failed: " << ownerNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (MkFileWithPatternRespMsg*)respMsg;

   mkFileRes = (FhgfsOpsErr)respMsgCast->getResult();
   if(mkFileRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Node encountered an error: " << FhgfsOpsErrTk::toErrString(mkFileRes) <<
         std::endl;
      goto err_cleanup;
   }

   retVal = true;

err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}
