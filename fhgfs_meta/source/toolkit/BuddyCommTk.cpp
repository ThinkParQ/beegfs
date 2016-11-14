#include <program/Program.h>
#include <components/buddyresyncer/BuddyResyncer.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>

#include "BuddyCommTk.h"

namespace BuddyCommTk
{
   static const std::string BUDDY_NEEDS_RESYNC_FILENAME        = "buddyneedsresync";

   void checkBuddyNeedsResync()
   {
      App* app = Program::getApp();
      MirrorBuddyGroupMapper* metaBuddyGroups = app->getMetaBuddyGroupMapper();
      TargetStateStore* metaNodeStates = app->getMetaStateStore();
      InternodeSyncer* internodeSyncer = app->getInternodeSyncer();
      BuddyResyncer* buddyResyncer = app->getBuddyResyncer();

      NumNodeID localID = app->getLocalNodeNumID();
      bool isPrimary;
      NumNodeID buddyID = NumNodeID(metaBuddyGroups->getBuddyTargetID(localID.val(), &isPrimary) );

      if (isPrimary) // Only do the check if we are the primary.
      {
         const bool buddyNeedsResyncFileExists = getBuddyNeedsResync();

         if (buddyNeedsResyncFileExists)
         {
            LOG_DEBUG(__func__, Log_NOTICE, "buddyneedsresync file found.");

            CombinedTargetState state = CombinedTargetState(TargetReachabilityState_ONLINE,
               TargetConsistencyState_NEEDS_RESYNC);
            metaNodeStates->getState(buddyID.val(), state);

            // Only send message if buddy was still reported as GOOD before (otherwise the mgmtd
            // already knows it needs a resync, or it's BAD and shouldn't be resynced anyway).
            if (state.consistencyState == TargetConsistencyState_GOOD)
            {
               setBuddyNeedsResyncState(true);
               LogContext(__func__).log(Log_NOTICE, "Set needs-resync state for buddy node.");
            }
         }

         // check if the secondary is set to needs-resync by the mgmtd.
         TargetConsistencyState consistencyState = internodeSyncer->getNodeConsistencyState();

         // If our own state is not good, don't start resync (wait until InternodeSyncer sets us
         // good again).
         if (consistencyState != TargetConsistencyState_GOOD)
         {
            LOG_DEBUG(__func__, Log_DEBUG,
               "Local node state is not good, won't check buddy state.");
            return;
         }

         CombinedTargetState buddyState;
         if (!metaNodeStates->getState(buddyID.val(), buddyState) )
         {
            LOG_DEBUG(__func__, Log_DEBUG, "Buddy state is invalid for node ID "
               + buddyID.str() + ".");
            return;
         }

         if (buddyState == CombinedTargetState(TargetReachabilityState_ONLINE,
             TargetConsistencyState_NEEDS_RESYNC) )
         {
            FhgfsOpsErr resyncRes = buddyResyncer->startResync();

            if (resyncRes == FhgfsOpsErr_SUCCESS)
               LogContext(__func__).log(Log_WARNING, "Starting buddy resync job.");
            else if (resyncRes == FhgfsOpsErr_INUSE)
               LogContext(__func__).log(Log_DEBUG, "Resync job currently running.");
            else
               LogContext(__func__).log(Log_WARNING, "Starting buddy resync job failed.");
         }
      }
   }

   /**
    * Creates the buddyneedsresync file, and tells the buddy about the state.
    */
   void setBuddyNeedsResync(const std::string& path, bool needsResync)
   {
      if (needsResync)
      {
         bool fileCreated;
         createBuddyNeedsResyncFile(path, fileCreated);
         if (fileCreated)
            setBuddyNeedsResyncState(needsResync);
      }
      else
      {
         removeBuddyNeedsResyncFile(path);
         setBuddyNeedsResyncState(needsResync);
      }
   }

   /**
    * Tells the buddy about the state, does NOT create the buddyneedsresync file.
    */
   void setBuddyNeedsResyncState(bool needsResync)
   {
      App* app = Program::getApp();
      NodeStore* mgmtNodes = app->getMgmtNodes();
      MirrorBuddyGroupMapper* mbg = app->getMetaBuddyGroupMapper();

      const NumNodeID localNodeID = app->getLocalNodeNumID();
      const NumNodeID buddyNodeID(mbg->getBuddyTargetID(localNodeID.val() ) );

      auto mgmtNode = mgmtNodes->referenceFirstNode();
      if (!mgmtNode)
      {
         LogContext(__func__).logErr("Management node not defined.");
         return;
      }

      TargetConsistencyState stateToSet = needsResync
         ? TargetConsistencyState_NEEDS_RESYNC
         : TargetConsistencyState_GOOD;

      UInt16List targetIDList(1, buddyNodeID.val() );
      UInt8List stateList(1, stateToSet);

      SetTargetConsistencyStatesMsg msg(NODETYPE_Meta, &targetIDList, &stateList, false);

      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      bool sendRes = MessagingTk::requestResponse(*mgmtNode, &msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp, &respBuf, &respMsg);

      if (!sendRes)
      {
         LogContext(__func__).log(Log_WARNING,
            "Could not reach managemend node trying to set needs-resync state for node "
            + buddyNodeID.str() + ".");
      }
      else
      {
         SetTargetConsistencyStatesRespMsg* respMsgCast =
            static_cast<SetTargetConsistencyStatesRespMsg*>(respMsg);

         if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
            LogContext(__func__).log(Log_CRITICAL,
               "Management node did not accept needs-resync state for node "
               + buddyNodeID.str() + ".");
      }
   }

   /**
    * @param outFileCreated set to true if file didn't exist before and was actually created.
    */
   void createBuddyNeedsResyncFile(const std::string& path, bool& outFileCreated)
   {
      std::string fileName = path + "/" + BUDDY_NEEDS_RESYNC_FILENAME;

      int createErrno = 0;
      bool createRes = StorageTk::createFile(fileName, &createErrno, &outFileCreated);

      if (!createRes)
      {
         LogContext(__func__).logErr("Unable to create file for needed buddy resync."
            " SysErr: " + System::getErrString(createErrno) );
         return;
      }

      if (outFileCreated)
      {
         // File wasn't there before - inform user about newly needed resync.
         LogContext(__func__).log(Log_CRITICAL,
            "Marked secondary buddy for needed resync.");
      }
   }

   void removeBuddyNeedsResyncFile(const std::string& path)
   {
      const std::string filename = path + "/" + BUDDY_NEEDS_RESYNC_FILENAME;
      const int unlinkRes = unlink(filename.c_str() );
      if (unlinkRes != 0 && errno != ENOENT)
      {
         LogContext(__func__).logErr("Unable to remove file for needed buddy resync."
            " SysErr: " + System::getErrString(errno) );
      }
   }

   bool getBuddyNeedsResync()
   {
      const std::string metaPath = Program::getApp()->getMetaPath();
      const std::string fileName = metaPath + "/" + BUDDY_NEEDS_RESYNC_FILENAME;
      return StorageTk::pathExists(fileName);
   }
};

