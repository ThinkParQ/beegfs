#ifndef MODEHELPER_H_
#define MODEHELPER_H_

#include <common/storage/mirroring/BuddyResyncJobStatistics.h>
#include <common/nodes/Node.h>
#include <common/Common.h>


#define MODE_ARG_NODETYPE              "--nodetype"
#define MODE_ARG_NODETYPE_CLIENT       "client"
#define MODE_ARG_NODETYPE_META         "metadata"
#define MODE_ARG_NODETYPE_META_SHORT   "meta"
#define MODE_ARG_NODETYPE_STORAGE      "storage"
#define MODE_ARG_NODETYPE_MGMT         "management"
#define MODE_ARG_NODETYPE_MGMT_SHORT   "mgmt"


class ModeHelper
{
   public:
      static bool checkRootPrivileges();
      static NodeType nodeTypeFromCfg(StringMap* cfg, bool* outWasSet = NULL);
      static bool makePathRelativeToMount(std::string path, bool useParent,
         bool resolveFinalSymlink, std::string* inOutMountRoot, std::string* outRelativePath);
      static bool checkInvalidArgs(StringMap* cfg);

      static bool registerNode();
      static bool unregisterNode();

      static bool pathIsOnFhgfs(std::string& path);
      static bool fdIsOnFhgfs(int fd);

      static void registerInterruptSignalHandler();
      static void signalHandler(int sig);

      static void blockInterruptSigs(sigset_t* oldMask);
      static void blockAllSigs(sigset_t* oldMask);
      static void restoreSigs(sigset_t* oldMask);
      static bool isInterruptPending();

      static FhgfsOpsErr getStorageBuddyResyncStats(uint16_t targetID,
            StorageBuddyResyncJobStatistics& outStats);
      static FhgfsOpsErr getMetaBuddyResyncStats(uint16_t nodeID,
            MetaBuddyResyncJobStatistics& outStats);

   private:
      ModeHelper() {};

      static bool getMountRoot(std::string realPath, std::string* outMountRoot);
};

#endif /* MODEHELPER_H_ */
