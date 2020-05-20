#include <app/App.h>
#include <common/net/message/storage/listing/ListDirFromOffsetMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/storage/Metadata.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/DisposalCleaner.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeDisposeUnusedFiles.h"


#define NODEINFO_INDENTATION_STR   "   "

#define MODEDISPOSEUNUSED_ARG_PRINTNODES  "--printnodes"
#define MODEDISPOSEUNUSED_ARG_PRINTFILES  "--printfiles"
#define MODEDISPOSEUNUSED_ARG_PRINTSTATS  "--printstats"
#define MODEDISPOSEUNUSED_ARG_DISPOSE     "--dispose"


int ModeDisposeUnusedFiles::execute()
{
   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   if(!readConfig() )
      return APPCODE_RUNTIME_ERROR;

   // walk over nodes and files
   handleNodes(metaNodes);


   if(cfgPrintStats)
   {
      std::cout << "===" << std::endl;
      std::cout << "Disposed / Total: " << numUnlinkedTotal << "/" << numEntriesTotal << std::endl;
   }

   return APPCODE_NO_ERROR;
}

void ModeDisposeUnusedFiles::printHelp()
{
   std::cout << std::endl;
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --printnodes            Print scanned metadata nodes." << std::endl;
   std::cout << "  --printfiles            Print file IDs that are marked for disposal." << std::endl;
   std::cout << "  --printstats            Print statistics (number of files, ...)." << std::endl;
   std::cout << "  --dispose               Try to remove the marked files." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode purges remains of old files, which have been unlinked by the user" << std::endl;
   std::cout << " and are no longer needed. (There is normally no need to use this mode.)" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: List disposable files without actually purging them" << std::endl;
   std::cout << "  # beegfs-ctl --disposeunused --printfiles --printstats" << std::endl;
}

bool ModeDisposeUnusedFiles::readConfig()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   iter = cfg->find(MODEDISPOSEUNUSED_ARG_PRINTNODES);
   if(iter != cfg->end() )
   {
      cfgPrintNodes = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEDISPOSEUNUSED_ARG_PRINTFILES);
   if(iter != cfg->end() )
   {
      cfgPrintFiles = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEDISPOSEUNUSED_ARG_PRINTSTATS);
   if(iter != cfg->end() )
   {
      cfgPrintStats = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEDISPOSEUNUSED_ARG_DISPOSE);
   if(iter != cfg->end() )
   {
      cfgUnlinkFiles = true;
      cfg->erase(iter);
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
      return false;

   // set default options, if noting was selected by the user
   if(!cfgPrintNodes && !cfgPrintFiles && !cfgPrintStats && !cfgUnlinkFiles)
   {
      cfgPrintNodes = true;
      cfgPrintStats = true;
   }

   return true;
}

FhgfsOpsErr ModeDisposeUnusedFiles::handleItem(Node& owner, const std::string& entryID,
   const bool isMirrored)
{
   if (currentNode != &owner)
   {
      printStats();

      if (cfgPrintNodes)
      {
         if (currentNode)
            std::cout << "---" << std::endl << std::endl;

         std::cout << "Node: " << owner.getNumID() << std::endl;
      }

      numEntriesReceived = 0;
   }

   currentNode = &owner;
   numEntriesTotal += 1;
   numEntriesReceived += 1;

   if (cfgPrintFiles)
   {
      if (cfgPrintNodes)
         std::cout << NODEINFO_INDENTATION_STR;

      std::cout << entryID << std::endl;
   }

   if (cfgUnlinkFiles)
   {
      auto err = DisposalCleaner::unlinkFile(owner, entryID, isMirrored);

      if (err == FhgfsOpsErr_COMMUNICATION)
         std::cerr << "[" << entryID <<  "] Communication error" << std::endl;
      else if (err == FhgfsOpsErr_INUSE)
         std::cerr << "[" << entryID <<  "] File still in use" << std::endl;
      else if (err != FhgfsOpsErr_SUCCESS)
         std::cerr << "[" << entryID << "] Node encountered an error: " << err << std::endl;
      else
         numUnlinkedTotal += 1;
   }

   return FhgfsOpsErr_SUCCESS;
}

void ModeDisposeUnusedFiles::printStats()
{
   if (currentNode && cfgPrintStats)
   {
      if (cfgPrintNodes)
         std::cout << NODEINFO_INDENTATION_STR;

      std::cout << "#Entries: " << numEntriesReceived << std::endl;
   }
}

void ModeDisposeUnusedFiles::handleError(Node& node, FhgfsOpsErr err)
{
   std::cerr << "Metadata server " << node.getNumID() << " encoutered an error: " << err << "\n";
}

void ModeDisposeUnusedFiles::handleNodes(NodeStoreServers* metaNodes)
{
   using namespace std::placeholders;

   DisposalCleaner dc(*Program::getApp()->getMetaMirrorBuddyGroupMapper());

   dc.run(metaNodes->referenceAllNodes(),
         std::bind(&ModeDisposeUnusedFiles::handleItem, this, _1, _2, _3),
         std::bind(&ModeDisposeUnusedFiles::handleError, this, _1, _2));

   printStats();
}
