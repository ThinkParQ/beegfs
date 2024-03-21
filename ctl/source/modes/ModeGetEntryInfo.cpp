#include <app/App.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/Raid10Pattern.h>
#include <common/toolkit/MetaStorageTk.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeGetEntryInfo.h"


#define MODEGETENTRYINFO_ARG_UNMOUNTEDPATH          "--unmounted"
#define MODEGETENTRYINFO_ARG_READFROMSTDIN          "-"
#define MODEGETENTRYINFO_ARG_NULLDELIM              "--null"
#define MODEGETENTRYINFO_ARG_VERBOSE                "--verbose"
#define MODEGETENTRYINFO_ARG_DONTSHOWTARGETMAPPINGS "--nomappings"


int ModeGetEntryInfo::execute()
{
   int retVal = APPCODE_NO_ERROR;
   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   // check arguments
   iter = cfg->find(MODEGETENTRYINFO_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      cfgUseMountedPath = false;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETENTRYINFO_ARG_VERBOSE);
   if(iter != cfg->end() )
   {
      cfgVerbose = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETENTRYINFO_ARG_DONTSHOWTARGETMAPPINGS);
   if(iter != cfg->end() )
   {
      cfgShowTargetMappings = false;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEGETENTRYINFO_ARG_NULLDELIM);
   if(iter != cfg->end() )
   {
      cfgNullDelim = true;
      cfg->erase(iter);
   }

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
      getPathFromStdin();

      if(this->cfgPathStr.empty() )
         return APPCODE_NO_ERROR; // no files given is not an error
   }

   do /* loop while we get paths from stdin (or terminate if stdin reading disabled) */
   {
      EntryInfo entryInfo;
      NodeHandle ownerNode;
      bool isFile;
      PathInfo pathInfo;
      bool getInfoRes;

      Path path(cfgPathStr);

      if (!ModeHelper::getEntryAndOwnerFromPath(path, cfgUseMountedPath,
            *metaNodes, app->getMetaRoot(), *app->getMetaMirrorBuddyGroupMapper(),
            entryInfo, ownerNode))
      {
         retVal = APPCODE_RUNTIME_ERROR;

         if (!cfgReadFromStdin)
            break;

         goto finish_this_entry;
      }

      isFile = DirEntryType_ISFILE(entryInfo.getEntryType() );

      // print some basic info

      std::cout << "Entry type: " << entryInfo.getEntryType() << std::endl;
      std::cout << "EntryID: " << entryInfo.getEntryID() << std::endl;
      if (!isFile && this->cfgVerbose)
         std::cout << "ParentID: " << entryInfo.getParentEntryID() << std::endl;

      if (!this->cfgVerbose)
      {
         if (entryInfo.getIsBuddyMirrored())
         {
            std::cout << "Metadata buddy group: " << entryInfo.getOwnerNodeID() << std::endl;
            std::cout << "Current primary metadata node: " << ownerNode->getTypedNodeID() << std::endl;
         }
         else
            std::cout << "Metadata node: " << ownerNode->getTypedNodeID() << std::endl;
      }

      // request and print entry info
      getInfoRes = getAndPrintEntryInfo(*ownerNode, &entryInfo, &pathInfo);
      if(!getInfoRes)
         retVal = APPCODE_RUNTIME_ERROR;

      if (this->cfgVerbose)
      {
         if (isFile)
         {
            std::string chunkPath =
               StorageTk::getFileChunkPath(&pathInfo, entryInfo.getEntryID() );

            std::cout << "Chunk path: " << chunkPath << std::endl;
         }

         std::cout << "Inlined inode: " << (entryInfo.getIsInlined() ? "yes" : "no") << std::endl;

         // compute and print dentry info for files and directories (except root dir)
         if (entryInfo.getEntryID() != "root")
         {
            EntryInfo parentEntryInfo;
            NodeHandle parentOwnerNode;

            Path parentDirPath = path.dirname();
            if (!ModeHelper::getEntryAndOwnerFromPath(parentDirPath, cfgUseMountedPath,
                  *metaNodes, app->getMetaRoot(), *app->getMetaMirrorBuddyGroupMapper(),
                  parentEntryInfo, parentOwnerNode))
            {
               retVal = APPCODE_RUNTIME_ERROR;

               if (!cfgReadFromStdin)
                  break;

               goto finish_this_entry;
            }

            std::string dentryPath = MetaStorageTk::getMetaDirEntryPath("", parentEntryInfo.getEntryID());

            std::cout << "Dentry info:" << std::endl;
            std::cout << " + Path: " << dentryPath.substr(1) << "/" << std::endl;
            this->printMetaNodeInfo(&parentEntryInfo, parentOwnerNode);
         }

         // print inode info for non-inlined inode(s)
         if (!entryInfo.getIsInlined())
         {
            std::string hashPath = MetaStorageTk::getMetaInodePath("", entryInfo.getEntryID());

            std::cout << "Inode info:" << std::endl;
            std::cout << " + Path: " << hashPath.substr(1) << std::endl;
            this->printMetaNodeInfo(&entryInfo, ownerNode);
         }
      }

      // cleanup
      if(!cfgReadFromStdin)
         break;


finish_this_entry:

      std::cout << std::endl;

      // read next relPath from stdin
      getPathFromStdin();

   } while(!this->cfgPathStr.empty() );

   return retVal;
}

void ModeGetEntryInfo::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                 Path to a file or directory." << std::endl;
   std::cout << "                         Specify \"-\" here to read multiple paths from" << std::endl;
   std::cout << "                         stdin (separated by newline or null (see --null))." << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --unmounted            If this is specified then the given path is relative" << std::endl;
   std::cout << "                         to the root directory of a possibly unmounted BeeGFS." << std::endl;
   std::cout << "                         (Symlinks will not be resolved in this case.)" << std::endl;
   std::cout << "  --verbose              Print more entry information, e.g. location on server." << std::endl;
   std::cout << "  --nomappings           Do not print to which storage servers the stripe" << std::endl;
   std::cout << "                         targets of a file are mapped." << std::endl;
   std::cout << "  --null                 Assume that paths on stdin are delimited by a null" << std::endl;
   std::cout << "                         character instead of newline." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode retrieves information about a certain file or directory, such as" << std::endl;
   std::cout << " striping information or the entry ID." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Retrieve information about a certain file" << std::endl;
   std::cout << "  $ beegfs-ctl --getentryinfo /mnt/beegfs/myfile" << std::endl;
}

bool ModeGetEntryInfo::getAndPrintEntryInfo(Node& ownerNode, EntryInfo* entryInfo,
   PathInfo* outPathInfo)
{
   NodeStoreServers* metaNodes = Program::getApp()->getMetaNodes();

   bool retVal = false;

   GetEntryInfoRespMsg* respMsgCast;

   FhgfsOpsErr getInfoRes;
   uint16_t mirrorNodeID;

   GetEntryInfoMsg getInfoMsg(entryInfo);

   const auto respMsg = MessagingTk::requestResponse(ownerNode, getInfoMsg,
         NETMSGTYPE_GetEntryInfoResp);
   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << ownerNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (GetEntryInfoRespMsg*)respMsg.get();

   getInfoRes = (FhgfsOpsErr)respMsgCast->getResult();
   if(getInfoRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error: " << ownerNode.getNodeIDWithTypeStr() << "; " <<
         "Error: " << getInfoRes << std::endl;
      goto err_cleanup;
   }

   *outPathInfo = *(respMsgCast->getPathInfo() );

   // print received information...

   // print metadata mirror node
   mirrorNodeID = respMsgCast->getMirrorNodeID();
   if(mirrorNodeID)
      std::cout << "Metadata mirror node: " << metaNodes->getTypedNodeID(NumNodeID(mirrorNodeID) )
         << std::endl;

   // print stripe pattern details
   printPattern(&respMsgCast->getPattern(), entryInfo->getEntryType());

   retVal = true;

err_cleanup:
   return retVal;
}

void ModeGetEntryInfo::printPattern(StripePattern* pattern, DirEntryType entryType)
{
   unsigned patternType = pattern->getPatternType();

   switch(patternType)
   {
      case StripePatternType_Raid0:
      case StripePatternType_Raid10:
      case StripePatternType_BuddyMirror:
      {
         const UInt16Vector* stripeTargetIDs = pattern->getStripeTargetIDs();
         const UInt16Vector* mirrorTargetIDs =
            (patternType == StripePatternType_Raid10) ?
               ( (Raid10Pattern*)pattern)->getMirrorTargetIDs() : NULL;

         std::cout << "Stripe pattern details:" << std::endl;

         std::cout << "+ Type: " << pattern->getPatternTypeStr() << std::endl;

         std::cout << "+ Chunksize: " << UnitTk::int64ToHumanStr(pattern->getChunkSize() ) <<
            std::endl;

         std::cout << "+ Number of storage targets: ";

         // print number of "desired" and "actual" targets

         if(stripeTargetIDs->empty() ) // print "desired"-only for dirs
            std::cout << "desired: " << pattern->getDefaultNumTargets();
         else
         if( (patternType == StripePatternType_Raid10) && !mirrorTargetIDs->empty() )
            std::cout << "desired: " << pattern->getDefaultNumTargets() << "; " <<
               "actual: " << stripeTargetIDs->size() << "+" << mirrorTargetIDs->size();
         else
            std::cout << "desired: " << pattern->getDefaultNumTargets() << "; " <<
               "actual: " << stripeTargetIDs->size();

         std::cout << std::endl;

         // storage pool (only displayed for directories)
         if (entryType == DirEntryType_DIRECTORY)
         {
            std::shared_ptr<StoragePool> storagePool =
               Program::getApp()->getStoragePoolStore()->getPool(pattern->getStoragePoolId());

            if (storagePool)
            {
               std::cout << "+ Storage Pool: " << storagePool->getId()
                         << " (" << storagePool->getDescription() << ")" << std::endl;
            }
         }

         /* note: we check size of stripeTargetIDs, because this might be a directory and thus
            it doesn't have any stripe targets */

         if (!stripeTargetIDs->empty())
         { // list storage targets
            if (patternType == StripePatternType_BuddyMirror)
               std::cout << "+ Storage mirror buddy groups:" << std::endl;
            else
               std::cout << "+ Storage targets:" << std::endl;

            UInt16VectorConstIter iter = stripeTargetIDs->begin();
            for( ; iter != stripeTargetIDs->end(); iter++)
            {
               std::cout << "  + " << *iter;

               if (patternType != StripePatternType_BuddyMirror)
                  printTargetMapping(*iter);

               std::cout << std::endl;
            }
         }

         if(patternType == StripePatternType_Raid10)
         { // list mirror targets (if any)

            if(!mirrorTargetIDs->empty() )
            {
               std::cout << "+ Mirror targets:" << std::endl;

               UInt16VectorConstIter iter = mirrorTargetIDs->begin();
               for( ; iter != mirrorTargetIDs->end(); iter++)
               {
                  std::cout << "  + " << *iter;

                  printTargetMapping(*iter);

                  std::cout << std::endl;
               }
            }
         }
      } break;

      default:
      {
         std::cerr << "Received an unknown stripe pattern type: " << patternType << std::endl;
      } break;
   } // end of switch
}

void ModeGetEntryInfo::printTargetMapping(uint16_t targetID)
{
   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();


   if(!cfgShowTargetMappings)
      return;

   std::cout << " @ ";

   // show to which server this targetID is mapped
   NumNodeID nodeNumID = targetMapper->getNodeID(targetID);
   if(!nodeNumID)
   {
      std::cout << "<unmapped>";
      return;
   }

   auto node = storageNodes->referenceNode(nodeNumID);
   if (!node)
   {
      std::cout << "<unknown(" << nodeNumID << ")>";
      return;
   }

   std::cout << node->getTypedNodeID();
}

void ModeGetEntryInfo::getPathFromStdin()
{
   this->cfgPathStr.clear();
   if(this->cfgNullDelim)
      getline(std::cin, this->cfgPathStr, '\0');
   else
      getline(std::cin, this->cfgPathStr);
}

void ModeGetEntryInfo::printMetaNodeInfo(EntryInfo* entryInfo, NodeHandle& ownerNode)
{
   if (entryInfo->getIsBuddyMirrored())
   {
      std::cout << " + Metadata buddy group: " << entryInfo->getOwnerNodeID().str() << std::endl;
      std::cout << " + Current primary metadata node: " << ownerNode->getTypedNodeID() << std::endl;
   }
   else
   {
      std::cout << " + Metadata node: " << ownerNode->getTypedNodeID() << std::endl;
   }
}
