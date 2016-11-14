#ifndef MODEREVERSELOOKUP_H_
#define MODEREVERSELOOKUP_H_

#include <common/Common.h>
#include <common/nodes/NodeStore.h>
#include "Mode.h"


class ModeReverseLookup : public Mode
{
   public:
      ModeReverseLookup()
      {
         cfgVerbose=false;
      }

      virtual int execute();

      static void printHelp();


   protected:

   private:
      bool cfgVerbose;

      bool getLinkOwnerInfo(std::string id, const std::vector<NodeHandle>& metaNodes,
         NumNodeID& outLinkOwnerID, std::string *outParentID);
      bool lookupId(std::string id, const std::vector<NodeHandle>& metaNodes, std::string *outName,
         std::string *outParentID);
      bool lookupIdOnSingleMDS(std::string id, Node* node,
         std::string *outName, std::string *outParentID);
      bool lookupIdOnSingleMDSFixedParent(std::string id, Node *node,
         std::string parentID, std::string *outName);

};

#endif /* MODEREVERSELOOKUP_H_ */
