#include <app/App.h>
#include <common/net/message/storage/creating/MkDirMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>

#include "ModeCreateDir.h"

#define MODECREATEDIR_ARG_NODES           "--nodes"
#define MODECREATEDIR_ARG_PERMISSIONS     "--access"
#define MODECREATEDIR_ARG_USERID          "--uid"
#define MODECREATEDIR_ARG_GROUPID         "--gid"
#define MODECREATEDIR_ARG_NOMIRROR        "--nomirror"
#define MODECREATEDIR_ARG_UNMOUNTEDPATH   "--unmounted"


int ModeCreateDir::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   DirSettings settings;

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   if(!initDirSettings(&settings) )
      return APPCODE_RUNTIME_ERROR;

   // find owner node
   NodeHandle ownerNode;
   EntryInfo entryInfo;

   {
      auto dirname = settings.path->dirname();
      if(!ModeHelper::getEntryAndOwnerFromPath(dirname, settings.useMountedPath,
            *metaNodes, app->getMetaRoot(), *metaBuddyGroupMapper, entryInfo, ownerNode))
      {
         retVal = APPCODE_RUNTIME_ERROR;
         goto cleanup_settings;
      }
   }

   // check that all preferred nodes exist
   // (we cannot check this in the meta server, since clients may have static files with preferred
   // nodes. breaking clients when unknown nodes are listed in such a file is not a good idea)
   if (settings.preferredNodes)
   {
      bool nodeFailures = false;

      for (auto it = settings.preferredNodes->begin(); it != settings.preferredNodes->end(); ++it)
      {
         const uint16_t node = *it;
         bool nodeExists = entryInfo.getIsBuddyMirrored() && !settings.noMirroring
            ? metaBuddyGroupMapper->getPrimaryTargetID(node) != 0
            : metaNodes->referenceNode(NumNodeID(node)) != nullptr;

         if (!nodeExists)
         {
            nodeFailures = true;
            std::cerr << "Unknown " << ((entryInfo.getIsBuddyMirrored() && !settings.noMirroring) ? "buddy group" : "node")
                 << " " << node << std::endl;
         }
      }

      if (nodeFailures)
      {
         retVal = APPCODE_RUNTIME_ERROR;
         goto cleanup_settings;
      }
   }

   // create the dir
   if(communicate(*ownerNode, &entryInfo, &settings) )
   {
      std::cout << "Operation succeeded." << std::endl;

      retVal = APPCODE_NO_ERROR;
   }

cleanup_settings:
   freeDirSettings(&settings);

   return retVal;
}

void ModeCreateDir::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                 Path of the new directory." << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --nodes=<nodelist>     Comma-separated list of metadata node IDs" << std::endl;
   std::cout << "                         to choose from for the new dir." << std::endl;
   std::cout << "                         When using mirroring, this is the buddy" << std::endl;
   std::cout << "                         mirror group id, rather than a node id." << std::endl;
   std::cout << "  --access=<mode>        The octal permissions value for user, " << std::endl;
   std::cout << "                         group and others. (Default: 0755)" << std::endl;
   std::cout << "  --uid=<userid_num>     User ID of the dir owner." << std::endl;
   std::cout << "  --gid=<groupid_num>    Group ID of the dir." << std::endl;
   std::cout << "  --nomirror             Do not mirror metadata, even if parent dir has" << std::endl;
   std::cout << "                         mirroring enabled." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode creates a new directory." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Create new directory on node id \"1\"" << std::endl;
   std::cout << "  # beegfs-ctl --createdir --nodes=1 /mnt/beegfs/mydir" << std::endl;
}

/**
 * Note: Remember to call freeDirSettings() when you're done.
 */
bool ModeCreateDir::initDirSettings(DirSettings* settings)
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   settings->preferredNodes = NULL;
   settings->path = NULL;

   StringMapIter iter;

   // parse and validate command line args

   settings->mode = 0755;
   iter = cfg->find(MODECREATEDIR_ARG_PERMISSIONS);
   if(iter != cfg->end() )
   {
      settings->mode = StringTk::strOctalToUInt(iter->second);
      settings->mode = settings->mode & 07777; // trim invalid flags from mode
      cfg->erase(iter);
   }

   settings->mode |= S_IFDIR; // make sure mode contains the "dir" flag

   settings->userID = 0;
   iter = cfg->find(MODECREATEDIR_ARG_USERID);
   if(iter != cfg->end() )
   {
      settings->userID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   settings->groupID = 0;
   iter = cfg->find(MODECREATEDIR_ARG_GROUPID);
   if(iter != cfg->end() )
   {
      settings->groupID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   settings->noMirroring = false;
   iter = cfg->find(MODECREATEDIR_ARG_NOMIRROR);
   if(iter != cfg->end() )
   {
      settings->noMirroring = true;
      cfg->erase(iter);
   }

   // parse preferred nodes

   StringList preferredNodesUntrimmed;
   settings->preferredNodes = new UInt16List();
   iter = cfg->find(MODECREATEDIR_ARG_NODES);
   if(iter != cfg->end() )
   {
      StringTk::explode(iter->second, ',', &preferredNodesUntrimmed);
      cfg->erase(iter);

      // trim nodes (copy to other list)
      for(StringListIter nodesIter = preferredNodesUntrimmed.begin();
          nodesIter != preferredNodesUntrimmed.end();
          nodesIter++)
      {
         std::string nodeTrimmedStr = StringTk::trim(*nodesIter);
         uint16_t nodeTrimmed = StringTk::strToUInt(nodeTrimmedStr);

         settings->preferredNodes->push_back(nodeTrimmed);
      }
   }

   settings->useMountedPath = true;
   iter = cfg->find(MODECREATEDIR_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      settings->useMountedPath = false;
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

      settings->path = new Path(pathStr);

      if(settings->path->empty() )
      {
         std::cerr << "Invalid path specified." << std::endl;
         return false;
      }
   }

   return true;
}

void ModeCreateDir::freeDirSettings(DirSettings* settings)
{
   SAFE_DELETE(settings->preferredNodes);
   SAFE_DELETE(settings->path);
}

std::string ModeCreateDir::generateServerPath(DirSettings* settings, std::string entryID)
{
   return entryID + "/" + settings->path->back();
}

bool ModeCreateDir::communicate(Node& ownerNode, EntryInfo* parentInfo, DirSettings* settings)
{
   bool retVal = false;

   MkDirRespMsg* respMsgCast;

   FhgfsOpsErr mkDirRes;

   // FIXME: Switch to ioctl

   std::string newDirName = settings->path->back();

   MkDirMsg msg(parentInfo, newDirName, settings->userID, settings->groupID, settings->mode, 0000,
      settings->preferredNodes);

   if(settings->noMirroring)
      msg.addMsgHeaderFeatureFlag(MKDIRMSG_FLAG_NOMIRROR);

   const auto respMsg = MessagingTk::requestResponse(ownerNode, msg, NETMSGTYPE_MkDirResp);
   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << ownerNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (MkDirRespMsg*)respMsg.get();

   mkDirRes = (FhgfsOpsErr)respMsgCast->getResult();
   if(mkDirRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Node encountered an error: " << mkDirRes << std::endl;
      goto err_cleanup;
   }

   retVal = true;

err_cleanup:
   return retVal;
}
