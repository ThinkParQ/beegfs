#ifndef MODEDISPOSEUNUSEDFILES_H_
#define MODEDISPOSEUNUSEDFILES_H_


#include <common/Common.h>
#include <common/nodes/NodeStoreServers.h>
#include "Mode.h"


class ModeDisposeUnusedFiles : public Mode
{
   public:
      ModeDisposeUnusedFiles()
      {
         cfgPrintNodes = false;
         cfgPrintFiles = false;
         cfgUnlinkFiles = false;
         cfgPrintStats = false;
         
         numEntriesTotal = 0;
         numUnlinkedTotal = 0;
      }
      
      virtual int execute();
   
      static void printHelp();


   private:
      bool cfgPrintNodes;
      bool cfgPrintFiles;
      bool cfgUnlinkFiles;
      bool cfgPrintStats;
      
      size_t numEntriesTotal; // total for all servers
      size_t numUnlinkedTotal; // total for all servers


      bool readConfig();
      void handleNodes(NodeStoreServers* metaNodes);
      bool getAndHandleFiles(Node& ownerNode);
      void handleEntries(Node& node, StringList& entryNames, bool buddyMirrored);
      bool unlinkEntry(Node& node, std::string entryName, bool buddyMirrored);
};


#endif /* MODEDISPOSEUNUSEDFILES_H_ */
