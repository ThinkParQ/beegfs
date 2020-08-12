#include <app/App.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoMsg.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeRefreshEntryInfo.h"


#define MODEREFRESHENTRYINFO_ARG_UNMOUNTEDPATH     "--unmounted"
#define MODEREFRESHENTRYINFO_ARG_READFROMSTDIN     "-"


int ModeRefreshEntryInfo::execute()
{
   int retVal = APPCODE_NO_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   // check arguments
   bool useMountedPath = true;
   iter = cfg->find(MODEREFRESHENTRYINFO_ARG_UNMOUNTEDPATH);
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

   if(pathStr == MODEREFRESHENTRYINFO_ARG_READFROMSTDIN)
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
      bool refreshInfoRes;

      // find owner node

      Path path(pathStr);

      NodeHandle ownerNode;
      EntryInfo entryInfo;

      if(!ModeHelper::getEntryAndOwnerFromPath(path, useMountedPath,
            *metaNodes, app->getMetaRoot(), *metaBuddyGroupMapper,
            entryInfo, ownerNode))
      {
         retVal = APPCODE_RUNTIME_ERROR;
         if(!cfgReadFromStdin)
            break;
         goto finish_this_entry;
      }

      // print some basic info

      std::cout << "Metadata node: " << ownerNode->getID() << std::endl;
      std::cout << "EntryID: " << entryInfo.getEntryID() << std::endl;


      // request and print entry info
      refreshInfoRes = refreshEntryInfo(*ownerNode, &entryInfo);
      if(!refreshInfoRes)
         retVal = APPCODE_RUNTIME_ERROR;

      // cleanup
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

void ModeRefreshEntryInfo::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                 Path to a file or directory." << std::endl;
   std::cout << "                         Specify \"-\" here to read multiple paths from" << std::endl;
   std::cout << "                         stdin (separated by newline)." << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --unmounted            If specified, the given path is relative to the root" << std::endl;
   std::cout << "                         directory of a possibly unmounted BeeGFS. (Symlinks" << std::endl;
   std::cout << "                         will not be resolved in this case.)" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode refreshes the metadata information for a certain file or directory" << std::endl;
   std::cout << " on the corresponding metadata server. (There is normally no need to use this" << std::endl;
   std::cout << " mode.)" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Refresh entry info for a certain file" << std::endl;
   std::cout << "  $ beegfs-ctl --refreshentryinfo /mnt/beegfs/myfile" << std::endl;
}

bool ModeRefreshEntryInfo::refreshEntryInfo(Node& ownerNode, EntryInfo* entryInfo)
{
   bool retVal = false;

   RefreshEntryInfoRespMsg* respMsgCast;

   FhgfsOpsErr refreshInfoRes;

   RefreshEntryInfoMsg refreshInfoMsg(entryInfo);

   const auto respMsg = MessagingTk::requestResponse(ownerNode, refreshInfoMsg,
         NETMSGTYPE_RefreshEntryInfoResp);
   if (!respMsg)
      goto err_cleanup;

   respMsgCast = (RefreshEntryInfoRespMsg*)respMsg.get();

   refreshInfoRes = (FhgfsOpsErr)respMsgCast->getValue();
   if(refreshInfoRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error: " << refreshInfoRes << std::endl;
      goto err_cleanup;
   }

   // print success message
   std::cout << "Refresh complete." << std::endl;

   retVal = true;

err_cleanup:
   return retVal;
}

