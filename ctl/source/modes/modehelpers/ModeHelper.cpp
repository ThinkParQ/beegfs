#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/RemoveNodeMsg.h>
#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/storage/lookup/LookupIntentMsg.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/net/message/storage/mirroring/GetStorageResyncStatsMsg.h>
#include <common/net/message/storage/mirroring/GetStorageResyncStatsRespMsg.h>
#include <common/net/message/storage/mirroring/GetMetaResyncStatsMsg.h>
#include <common/net/message/storage/mirroring/GetMetaResyncStatsRespMsg.h>
#include <common/nodes/NodeStore.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MetadataTk.h>
#include <modes/modehelpers/ModeInterruptedException.h>
#include <program/Program.h>
#include "ModeHelper.h"

#include <sys/vfs.h>
#include <toolkit/IoctlTk.h>

#include <boost/lexical_cast.hpp>


#define BEEGFS_MAGIC      0x19830326 /* some random number to identify fhgfs (fs type) */
                                    /* (originally defined in client FhgfsOpsSuper.h) */

bool ModeHelper::checkRootPrivileges()
{
   if(geteuid() && getegid() )
   { // no root privileges
      std::cerr << "This mode requires root privileges." << std::endl;
      return false;
   }

   return true;
}

/**
 * Reads the MODE_ARG_NODETYPE cfg entry to determine the node type.
 *
 * Note: Removes the corresponding key from cfg.
 */
NodeType ModeHelper::nodeTypeFromCfg(StringMap* cfg, bool* outWasSet)
{
   StringMapIter iter = cfg->find(MODE_ARG_NODETYPE);
   if(iter == cfg->end() )
   {
      if (outWasSet != NULL)
         *outWasSet = false;
      return NODETYPE_Invalid;
   }

   if (outWasSet != NULL)
      *outWasSet = true;

   std::string nodeTypeStr = iter->second;

   cfg->erase(iter);

   if(!strcasecmp(nodeTypeStr.c_str(), MODE_ARG_NODETYPE_CLIENT) )
      return NODETYPE_Client;
   else
   if(!strcasecmp(nodeTypeStr.c_str(), MODE_ARG_NODETYPE_META) )
      return NODETYPE_Meta;
   else
   if(!strcasecmp(nodeTypeStr.c_str(), MODE_ARG_NODETYPE_META_SHORT) )
      return NODETYPE_Meta;
   else
   if(!strcasecmp(nodeTypeStr.c_str(), MODE_ARG_NODETYPE_STORAGE) )
      return NODETYPE_Storage;
   else
   if(!strcasecmp(nodeTypeStr.c_str(), MODE_ARG_NODETYPE_MGMT) )
      return NODETYPE_Mgmt;
   else
   if(!strcasecmp(nodeTypeStr.c_str(), MODE_ARG_NODETYPE_MGMT_SHORT) )
      return NODETYPE_Mgmt;

   return NODETYPE_Invalid;
}


/**
 * Check given config for a remaining arg and print it as invalid.
 *
 * @return true if a remaining invalid argument was found in the given config.
 */
bool ModeHelper::checkInvalidArgs(StringMap* cfg)
{
   if(cfg->empty() )
      return false;

   if(cfg->size() == 1)
      std::cerr << "Invalid argument: " << cfg->begin()->first << std::endl;
   else
   { // multiple invalid args
      std::cerr << "Found " << cfg->size() << " invalid arguments. One of them is: " <<
         cfg->begin()->first << std::endl;
   }

   std::cerr << "Use \"" << RUNMODE_HELP_KEY_STRING << "\" as argument "
      "to show usage information." << std::endl;

   return true;
}

/**
 * @return true if an ack was received for the heartbeat, false otherwise
 */
bool ModeHelper::registerNode()
{
   const char* logContext = "Node Registration";
   NodeStore* mgmtNodes = Program::getApp()->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   DatagramListener* dgramLis = Program::getApp()->getDatagramListener();
   Node& localNode = Program::getApp()->getLocalNode();
   NicAddressList nicList(localNode.getNicList() );

   HeartbeatMsg msg(localNode.getID(), NumNodeID(), NODETYPE_Client, &nicList);
   msg.setPorts(dgramLis->getUDPPort(), 0);

   bool nodeRegistered = dgramLis->sendToNodeUDPwithAck(mgmtNode, &msg);

   if(nodeRegistered)
      LogContext(logContext).log(2, "Node registration successful.");
   else
      LogContext(logContext).log(1, "Node registration not successful. Management node offline?");

   return nodeRegistered;
}

/**
 * @return true if communication succeeded and mgmt daemon replied with "success", false otherwise
 */
bool ModeHelper::unregisterNode()
{
   const char* logContext = "Node Deregistration";
   NodeStore* mgmtNodes = Program::getApp()->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   bool nodeUnregistered = false;
   Node& localNode = Program::getApp()->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   RemoveNodeRespMsg* respMsgCast;
   FhgfsOpsErr serverResult;

   RemoveNodeMsg msg(localNodeNumID, NODETYPE_Client);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg, NETMSGTYPE_RemoveNodeResp);
   if (!respMsg)
   {
      LogContext(logContext).logErr("Communication error during node deregistration.");
      goto err_cleanup;
   }

   respMsgCast = (RemoveNodeRespMsg*)respMsg.get();
   serverResult = (FhgfsOpsErr)respMsgCast->getValue();

   if(serverResult != FhgfsOpsErr_SUCCESS)
   { // deregistration failed
      LogContext(logContext).log(2, std::string("Node deregistration failed: ") +
         boost::lexical_cast<std::string>(serverResult));
   }
   else
   { // deregistration successful
      LogContext(logContext).log(2, "Node deregistration successful.");

      nodeUnregistered = true;
   }

err_cleanup:
   return nodeUnregistered;
}

/**
   msg.setMsgHeaderTargetID(targetID);

 * Check if the given path is on FhGFS.
 *
 * @return  - 'true' -> is on FhGFS, 'false' -> is not or something failed during detection
 */
bool ModeHelper::pathIsOnFhgfs(std::string& path)
{
   /* we also could use statfs(), but statfs() is far more expensive, it has to do network
    * communication and also to ask all servers for their current storage usage, etc.
    * The ioctl only needs to check the mounted filesystem and to return the FhGFS magic...
    */
   IoctlTk Fhgfsioctl(path);
   return Fhgfsioctl.testIsFhGFS();
}

/**
 * Check if the given file descriptor is on FhGFS.
 *
 * @return  - 'true' -> is on FhGFS, 'false' -> is not or something failed during detection
 */
bool ModeHelper::fdIsOnFhgfs(int fd)
{
   /* we also could use statfs(), but statfs() is far more expensive, it has to do network
    * communication and also to ask all servers for their current storage usage, etc.
    * The ioctl only needs to check the mounted filesystem and to return the FhGFS magic...
    */
   IoctlTk Fhgfsioctl(fd);
   return Fhgfsioctl.testIsFhGFS();
}


void ModeHelper::registerInterruptSignalHandler()
{
   signal(SIGINT, ModeHelper::signalHandler);
   signal(SIGTERM, ModeHelper::signalHandler);
}


void ModeHelper::signalHandler(int sig)
{
   switch(sig)
   {
      case SIGINT:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
      } break;

      case SIGTERM:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
      } break;

      default:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
      } break;
   }

   throw ModeInterruptedException("Interrupted by signal");
}

/**
 * Block interrupt signals
 */
void ModeHelper::blockInterruptSigs(sigset_t* oldMask)
{
   sigset_t blockingMask;
   sigemptyset(&blockingMask);
   sigaddset(&blockingMask, SIGINT);
   sigaddset(&blockingMask, SIGTERM);

   sigemptyset(oldMask);

   int sigRes = pthread_sigmask(SIG_SETMASK, &blockingMask, oldMask);

#ifdef BEEGFS_DEBUG
   if (sigRes)
      std::cerr << "Setting the complete blocking mask failed!" << std::endl;
#else
   IGNORE_UNUSED_VARIABLE(sigRes);
#endif
}

/**
 * Block all signals
 */
void ModeHelper::blockAllSigs(sigset_t* oldMask)
{
   sigset_t blockingMask;
   sigfillset(&blockingMask);

   sigemptyset(oldMask);

   int sigRes = pthread_sigmask(SIG_SETMASK, &blockingMask, oldMask);

#ifdef BEEGFS_DEBUG
   if (sigRes)
      std::cerr << "Setting the complete blocking mask failed!" << std::endl;
#else
   IGNORE_UNUSED_VARIABLE(sigRes);
#endif
}


/**
 * Restore a saved signal mask
 */
void ModeHelper::restoreSigs(sigset_t* oldMask)
{
   // restore signals
   int sigRes = pthread_sigmask(SIG_SETMASK, oldMask, NULL); // restore to previous value
   #ifdef BEEGFS_DEBUG
      if (sigRes)
         std::cerr << "Restoring the blocking mask failed!" << std::endl;
   #endif
   IGNORE_UNUSED_VARIABLE(sigRes);

}

/**
 * Check if there is any signal to stop/abort is pending
 */
bool ModeHelper::isInterruptPending()
{
   sigset_t pendingSigs;
   int getPendingRes = sigpending(&pendingSigs);
   if (getPendingRes)
      std::cerr << "Failed to check for pending signals" << std::endl;

   if (sigismember(&pendingSigs, SIGINT) )
   {
      // pendinging signals, do nothing
      return true;
   }
   else
   if (sigismember(&pendingSigs, SIGTERM) )
   {
      // pendinging signals, do nothing
      return true;
   }

   return false;
}

FhgfsOpsErr ModeHelper::getStorageBuddyResyncStats(uint16_t targetID,
      StorageBuddyResyncJobStatistics& outStats)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStore* nodes = app->getStorageNodes();

   NumNodeID nodeID = targetMapper->getNodeID(targetID);

   auto node = nodes->referenceNode(nodeID);

   if (!node)
      return FhgfsOpsErr_UNKNOWNNODE;

   GetStorageResyncStatsMsg msg(targetID);

   const auto respMsg = MessagingTk::requestResponse(*node, msg,
         NETMSGTYPE_GetStorageResyncStatsResp);
   if (!respMsg)
   {
      retVal = FhgfsOpsErr_COMMUNICATION;
   }
   else
   {
      GetStorageResyncStatsRespMsg* respMsgCast = (GetStorageResyncStatsRespMsg*)respMsg.get();
      respMsgCast->getJobStats(outStats);
   }

   return retVal;
}

FhgfsOpsErr ModeHelper::getMetaBuddyResyncStats(uint16_t nodeID,
      MetaBuddyResyncJobStatistics& outStats)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   NodeStore* nodes = app->getMetaNodes();

   auto node = nodes->referenceNode(NumNodeID(nodeID));

   if (!node)
      return FhgfsOpsErr_UNKNOWNNODE;

   GetMetaResyncStatsMsg msg;

   const auto respMsg = MessagingTk::requestResponse(*node, msg, NETMSGTYPE_GetMetaResyncStatsResp);
   if (!respMsg)
   {
      retVal = FhgfsOpsErr_COMMUNICATION;
   }
   else
   {
      GetMetaResyncStatsRespMsg* respMsgCast = (GetMetaResyncStatsRespMsg*)respMsg.get();
      respMsgCast->getJobStats(outStats);
   }

   return retVal;
}

bool ModeHelper::getEntryAndOwnerFromPath(Path& path, bool useMountedPath,
      NodeStoreServers& metaNodes, const RootInfo& metaRoot,
      MirrorBuddyGroupMapper& metaBuddyGroupMapper,
      EntryInfo& outEntryInfo, NodeHandle& outOwnerHandle)
{
   if(useMountedPath)
   {
      struct stat s;

      if (lstat(path.str().c_str(), &s)) {
         perror("Stat failed");
         return false;
      }

      const bool doLookup = !(S_ISDIR(s.st_mode) || S_ISREG(s.st_mode));
      std::string testPath = doLookup
         ? StorageTk::getPathDirname(path.str())
         : path.str();

      IoctlTk ioctlTk(testPath);
      if(!ioctlTk.getEntryInfo(outEntryInfo))
      {
         std::cerr << "Unable to retrieve entryInfo for " << testPath << ": "
                   << ioctlTk.getErrMsg() << std::endl;
         return false;
      }

      auto getNodeID = [&metaBuddyGroupMapper] (const EntryInfo& e) -> NumNodeID {
         return e.getIsBuddyMirrored() ?
            NumNodeID(metaBuddyGroupMapper.getPrimaryTargetID(e.getOwnerNodeID().val())) :
            e.getOwnerNodeID();
      };

      if (doLookup) {
         LookupIntentMsg msg(&outEntryInfo, path.back());
         auto node = metaNodes.referenceNode(getNodeID(outEntryInfo));

         const auto resp = MessagingTk::requestResponse(*node, msg, NETMSGTYPE_LookupIntentResp);
         auto& respMsg = static_cast<LookupIntentRespMsg&>(*resp);

         if (respMsg.getLookupResult() != FhgfsOpsErr_SUCCESS)
            return false;
         outEntryInfo = *respMsg.getEntryInfo();
      }

      outOwnerHandle = metaNodes.referenceNode(getNodeID(outEntryInfo));
      if (!outOwnerHandle)
      {
         // This is usually an indication that the ctl was called with a
         // configuration file that doesn't match the filesystem it is trying to
         // get the info for and will lead to a segmentation violation further
         // along when this pointer is dereferenced. So we print a hint and
         // return an error.
         std::cerr << "The metadata node that owns the directory" << std::endl;
         std::cerr << "  " << path.str() << std::endl;
         std::cerr << "could not be determined." << std::endl;
         std::cerr << std::endl;

         std::cerr << "Please make sure to supply the correct client configuration file (\"--cfgFile=\")" << std::endl;
         std::cerr << "for the mount point the directory is on or to use the \"--mount=\" option." << std::endl;
         std::cerr << "By default, beegfs-ctl uses /etc/beegfs/beegfs-client.conf to determine which" << std::endl;
         std::cerr << "filesystem to connect to. See \"beegfs-ctl --help\" for more information." << std::endl;
         return false;
      }
   }
   else
   {
      FhgfsOpsErr findRes = MetadataTk::referenceOwner(&path, &metaNodes, outOwnerHandle,
            &outEntryInfo, metaRoot, &metaBuddyGroupMapper);
      if(findRes != FhgfsOpsErr_SUCCESS)
      {
         std::cerr << "Unable to find metadata node for path: " << path <<
            std::endl;
         std::cerr << "Error: " << findRes << std::endl;
         return false;
      }
   }

   return true;
}

void ModeHelper::printEnterpriseFeatureMsg()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   
   if (cfg->getSysNoEnterpriseFeatureMsg())
      return;

   std::cerr << "--------------------------------------------------------------------------------" << std::endl;
   std::cerr << "| BeeGFS Enterprise Feature                                                    |" << std::endl;
   std::cerr << "|                                                                              |" << std::endl;
   std::cerr << "| This beegfs-ctl mode configures a BeeGFS Enterprise Feature.                 |" << std::endl;
   std::cerr << "|                                                                              |" << std::endl;
   std::cerr << "| By downloading and/or installing BeeGFS, you have agreed to the EULA of      |" << std::endl;
   std::cerr << "| BeeGFS: https://www.beegfs.io/docs/BeeGFS_EULA.txt                           |" << std::endl;
   std::cerr << "|                                                                              |" << std::endl;
   std::cerr << "| Please note that any use of Enterprise Features of BeeGFS for longer than    |" << std::endl;
   std::cerr << "| the trial period of 60 (sixty) days requires a valid License & Support       |" << std::endl;
   std::cerr << "| Agreement with the licensor of BeeGFS \"ThinkParQ GmbH\".                      |" << std::endl;
   std::cerr << "|                                                                              |" << std::endl;
   std::cerr << "| Contact: sales@thinkparq.com                                                 |" << std::endl;
   std::cerr << "| Thank you for supporting BeeGFS development!                                 |" << std::endl;
   std::cerr << "|                                                                              |" << std::endl;
   std::cerr << "| If you are using BeeGFS in conformity with the EULA and do not wish to see   |" << std::endl;
   std::cerr << "| this message in the future, you can set sysNoEnterpriseFeatureMsg to true in |" << std::endl;
   std::cerr << "| beegfs-client.conf to disable it.                                            |" << std::endl;
   std::cerr << "--------------------------------------------------------------------------------" << std::endl;
   std::cerr << std::endl;
}
