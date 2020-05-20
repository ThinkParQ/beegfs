#ifndef DISPOSALCLEANER_H
#define DISPOSALCLEANER_H

#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/Node.h>
#include <functional>

class DisposalCleaner
{
   public:
      typedef FhgfsOpsErr (OnItemFn)(Node& owner, const std::string& entryID,
            const bool isMirrored);
      typedef void (OnErrorFn)(Node& node, FhgfsOpsErr err);

      DisposalCleaner(MirrorBuddyGroupMapper& bgm, bool onlyMirrored=false):
         bgm(&bgm), onlyMirrored(onlyMirrored)
      {
      }

      void run(const std::vector<NodeHandle>& nodes, const std::function<OnItemFn>& onItem,
         const std::function<OnErrorFn>& onError,
         const std::function<bool()>& abortCondition = [] () { return false; });

      static FhgfsOpsErr unlinkFile(Node& node, std::string entryName, const bool isMirrored);

   private:
      MirrorBuddyGroupMapper* bgm;

      FhgfsOpsErr walkNode(Node& node, const std::function<OnItemFn>& onItem,
              const std::function<bool()>& abortCondition);

      const bool onlyMirrored;
};

#endif
