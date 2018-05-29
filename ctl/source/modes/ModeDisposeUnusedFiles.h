#ifndef MODEDISPOSEUNUSEDFILES_H_
#define MODEDISPOSEUNUSEDFILES_H_


#include <common/Common.h>
#include <common/nodes/NodeStoreServers.h>
#include "Mode.h"


class ModeDisposeUnusedFiles : public Mode
{
   public:
      ModeDisposeUnusedFiles():
         currentNode(nullptr)
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

      size_t numEntriesReceived; // per node

      Node* currentNode;


      bool readConfig();
      void handleNodes(NodeStoreServers* metaNodes);
      FhgfsOpsErr handleItem(Node& owner, const std::string& entryID, const bool isMirrored);
      void handleError(Node& node, FhgfsOpsErr err);

      void printStats();
};


#endif /* MODEDISPOSEUNUSEDFILES_H_ */
