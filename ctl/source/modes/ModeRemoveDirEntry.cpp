#include <app/App.h>
#include <common/net/message/storage/creating/RmDirEntryMsg.h>
#include <common/net/message/storage/creating/RmDirEntryRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeRemoveDirEntry.h"


#define MODEREMOVEDIRENTRY_ARG_UNMOUNTEDPATH     "--unmounted"
#define MODEREMOVEDIRENTRY_ARG_READFROMSTDIN     "-"


int ModeRemoveDirEntry::execute()
{
   int retVal = APPCODE_NO_ERROR;

   App* app = Program::getApp();

   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   // check arguments

   bool useMountedPath = true;
   iter = cfg->find(MODEREMOVEDIRENTRY_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      useMountedPath = false;
      cfg->erase(iter);
   }

   if(cfg->empty() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   std::string pathStr = cfg->begin()->first;
   cfg->erase(cfg->begin() );

   if(pathStr == MODEREMOVEDIRENTRY_ARG_READFROMSTDIN)
      cfgReadFromStdin = true;


   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   if(cfgReadFromStdin)
   { // read first path from stdin
      pathStr.clear();
      getline(std::cin, pathStr);

      if(!pathStr.length() )
         return APPCODE_NO_ERROR; // no files given is not an error
   }

   do /* loop while we get paths from stdin (or terminate if stdin reading disabled) */
   {
      bool rmLinkRes;

      // find owner node

      Path path(pathStr);

      NodeHandle ownerNode;
      EntryInfo entryInfo;

      {
         auto dirname = path.dirname();
         if (!ModeHelper::getEntryAndOwnerFromPath(dirname, useMountedPath,
               *metaNodes, app->getMetaRoot(), *metaBuddyGroupMapper,
               entryInfo, ownerNode))
         {
            retVal = APPCODE_RUNTIME_ERROR;
            if (!cfgReadFromStdin)
               break;
            goto finish_this_entry;
         }
      }

      // print some basic info

      std::cout << "Metadata node: " << ownerNode->getID() << std::endl;
      std::cout << "EntryID: " << entryInfo.getEntryID() << std::endl;


      // request and print entry info
      rmLinkRes = removeDirEntry(*ownerNode, &entryInfo, path.back() );
      if(!rmLinkRes)
         retVal = APPCODE_RUNTIME_ERROR;

      if(!cfgReadFromStdin)
         break;


finish_this_entry:

      std::cout << std::endl;

      // read next path from stdin
      pathStr.clear();
      getline(std::cin, pathStr);

   } while(pathStr.length() );

   return retVal;
}

void ModeRemoveDirEntry::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                 The path to file or directory." << std::endl;
   std::cout << "                         Specify \"-\" here to read multiple paths from" << std::endl;
   std::cout << "                         stdin (separated by newline)." << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --unmounted            If specified, the given path is relative to the root" << std::endl;
   std::cout << "                         directory of a possibly unmounted FhGFS. (Symlinks" << std::endl;
   std::cout << "                         will not be resolved in this case.)" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode removes the parent directory entry link of a certain file or" << std::endl;
   std::cout << " directory. (Don't use this unless you know what you are doing.)" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Remove entry link of a certain file" << std::endl;
   std::cout << "  $ beegfs-ctl --removedirentry /mnt/beegfs/myfile" << std::endl;
}

bool ModeRemoveDirEntry::removeDirEntry(Node& ownerNode, EntryInfo* parentInfo,
   std::string entryName)
{
   bool retVal = false;

   RmDirEntryRespMsg* respMsgCast;

   FhgfsOpsErr rmLinkRes;

   RmDirEntryMsg rmDirEntryMsg(parentInfo, entryName);

   const auto respMsg = MessagingTk::requestResponse(ownerNode, rmDirEntryMsg,
         NETMSGTYPE_RmDirEntryResp);
   if (!respMsg)
      goto err_cleanup;

   respMsgCast = (RmDirEntryRespMsg*)respMsg.get();

   rmLinkRes = (FhgfsOpsErr)respMsgCast->getValue();
   if(rmLinkRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error: " << rmLinkRes << std::endl;
      goto err_cleanup;
   }

   // print success message
   std::cout << "Removal succeeded." << std::endl;

   retVal = true;

err_cleanup:
   return retVal;
}

