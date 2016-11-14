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
      std::string mountRoot;
      bool rmLinkRes;

      if(useMountedPath)
      { // make path relative to mount root
         std::string relativePath;

         bool pathRelativeRes = ModeHelper::makePathRelativeToMount(
            pathStr, true, false, &mountRoot, &relativePath);
         if(!pathRelativeRes)
            return APPCODE_RUNTIME_ERROR;

         pathStr = relativePath;
      }

      // find owner node

      Path path("/" + pathStr);

      NodeHandle ownerNode;
      EntryInfo entryInfo;

      FhgfsOpsErr findRes = MetadataTk::referenceOwner(
         &path, true, metaNodes, ownerNode, &entryInfo, metaBuddyGroupMapper);

      if(findRes != FhgfsOpsErr_SUCCESS)
      {
         std::cerr << "Unable to find metadata node for path: " << pathStr << std::endl;
         std::cerr << "Error: " << FhgfsOpsErrTk::toErrString(findRes) << std::endl;
         retVal = APPCODE_RUNTIME_ERROR;

         if(!cfgReadFromStdin)
            break;

         goto finish_this_entry;
      }

      // print some basic info

      std::cout << "Path: " << pathStr << std::endl;

      if(useMountedPath)
         std::cout << "Mount: " << mountRoot << std::endl;

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

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   RmDirEntryRespMsg* respMsgCast;

   FhgfsOpsErr rmLinkRes;

   RmDirEntryMsg rmDirEntryMsg(parentInfo, entryName);

   // request/response
   commRes = MessagingTk::requestResponse(
      ownerNode, &rmDirEntryMsg, NETMSGTYPE_RmDirEntryResp, &respBuf, &respMsg);
   if(!commRes)
      goto err_cleanup;

   respMsgCast = (RmDirEntryRespMsg*)respMsg;

   rmLinkRes = (FhgfsOpsErr)respMsgCast->getValue();
   if(rmLinkRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error: " << FhgfsOpsErrTk::toErrString(rmLinkRes) <<
         std::endl;
      goto err_cleanup;
   }

   // print success message
   std::cout << "Removal succeeded." << std::endl;

   retVal = true;

err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

