#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/creating/MkLocalDirMsg.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <components/ModificationEventFlusher.h>
#include <program/Program.h>
#include <storage/PosixACL.h>
#include "RmDirMsgEx.h"
#include "MkDirMsgEx.h"


bool MkDirMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   entryID = StorageTk::generateFileID(app->getLocalNode().getNumID());

   BaseType::processIncoming(ctx);

   // update operation counters
   updateNodeOp(ctx, MetaOpCounter_MKDIR);

   return true;
}

std::unique_ptr<MirroredMessageResponseState> MkDirMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   auto result = isSecondary
      ? mkDirSecondary()
      : mkDirPrimary(ctx);

   if (result && result->getResult() != FhgfsOpsErr_SUCCESS)
      LOG_DBG(GENERAL, DEBUG, "Failed to create directory",
            ("parentID", getParentInfo()->getEntryID()),
            ("newDirName", getNewDirName()),
            ("error", result->getResult()));

   return result;
}

std::tuple<HashDirLock, FileIDLock, ParentNameLock> MkDirMsgEx::lock(EntryLockStore& store)
{
   HashDirLock hashLock;

   // during resync we must lock the hash dir of the new inode even if the inode will be created on
   // a different node because we select the target node for the inode only after we have locked our
   // structures.
   if (resyncJob && resyncJob->isRunning())
      hashLock = {&store, MetaStorageTk::getMetaInodeHash(entryID)};

   FileIDLock dirLock(&store, getParentInfo()->getEntryID(), true);
   ParentNameLock dentryLock(&store, getParentInfo()->getEntryID(), getNewDirName());

   return std::make_tuple(std::move(hashLock), std::move(dirLock), std::move(dentryLock));
}

std::unique_ptr<MkDirMsgEx::ResponseState> MkDirMsgEx::mkDirPrimary(ResponseContext& ctx)
{
   const char* logContext = "MkDirMsg (mkDirPrimary)";

   App* app =  Program::getApp();
   ModificationEventFlusher* modEventFlusher = app->getModificationEventFlusher();
   const bool modEventLoggingEnabled = modEventFlusher->isLoggingEnabled();
   MetaStore* metaStore = app->getMetaStore();
   Config* config = app->getConfig();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();
   NodeCapacityPools* metaCapacityPools;
   NumNodeID expectedOwnerID;

   const EntryInfo* const parentInfo = getParentInfo();
   const std::string& newName = getNewDirName();

   FhgfsOpsErr retVal;

   // not a good idea to use scoped locks here, because we don't have a well-defined scope with
   // only buddy mirrored paths here; directly use entrylockstore
   EntryLockStore* entryLockStore = Program::getApp()->getSessions()->getEntryLockStore();

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(parentInfo->getEntryID(),
      parentInfo->getIsBuddyMirrored(), true);
   if(!parentDir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS, EntryInfo());

   // check whether localNode owns this (parent) directory
   NumNodeID localNodeID = app->getLocalNodeNumID();
   bool isBuddyMirrored = parentDir->getIsBuddyMirrored()
      && !isMsgHeaderFeatureFlagSet(MKDIRMSG_FLAG_NOMIRROR);

   // check whether localNode owns this (parent) directory; if parentDir is buddy mirrored compare
   // ownership to buddy group id, otherwise to node id itself
   if (parentDir->getIsBuddyMirrored())
      expectedOwnerID = NumNodeID(metaBuddyGroupMapper->getLocalGroupID() );
   else
      expectedOwnerID = localNodeID;

   if (isBuddyMirrored)
      metaCapacityPools = app->getMetaBuddyCapacityPools();
   else
      metaCapacityPools = app->getMetaCapacityPools();

   if(parentDir->getOwnerNodeID() != expectedOwnerID)
   { // this node doesn't own the parent dir
      LogContext(logContext).logErr(std::string("Dir-owner mismatch: \"") +
         parentDir->getOwnerNodeID().str()  + "\" vs. \"" +
         expectedOwnerID.str() + "\"");
      metaStore->releaseDir(parentInfo->getEntryID() );
      return boost::make_unique<ResponseState>(FhgfsOpsErr_NOTOWNER, EntryInfo());
   }

   // choose new directory owner...
   unsigned numDesiredTargets = 1;
   unsigned minNumRequiredTargets = numDesiredTargets;
   UInt16Vector newOwnerNodes;

   metaCapacityPools->chooseStorageTargets(numDesiredTargets, minNumRequiredTargets,
                                           &getPreferredNodes(), &newOwnerNodes);

   if(unlikely(newOwnerNodes.size() < minNumRequiredTargets) )
   { // (might be caused by a bad list of preferred targets)
      LogContext(logContext).logErr("No metadata servers available for new directory: " + newName);

      metaStore->releaseDir(parentInfo->getEntryID() );
      // we know that *some* metadata server must exist, since we are obviously active when we get
      // here. most likely a client has received a pool update before we have, or we have been
      // switched from secondary to primary and haven't been set to Good yet.
      // if preferred nodes have been set (currently only done by ctl), those may also be registered
      // as unavailable at the current time.
      // have the client retry until things work out.
      return boost::make_unique<ResponseState>(FhgfsOpsErr_COMMUNICATION, EntryInfo());
   }

   const uint16_t ownerNodeID = newOwnerNodes[0];
   const std::string parentEntryID = parentInfo->getEntryID();
   int entryInfoFlags = isBuddyMirrored ? ENTRYINFO_FEATURE_BUDDYMIRRORED : 0;

   int mode = getMode();
   const int umask = getUmask();
   CharVector parentDefaultACLXAttr;
   CharVector accessACLXAttr;

   if (config->getStoreClientACLs())
   {
      // Determine the ACLs of the new directory.
      PosixACL parentDefaultACL;
      bool needsACL;
      FhgfsOpsErr parentDefaultACLRes;

      std::tie(parentDefaultACLRes, parentDefaultACLXAttr, std::ignore) = parentDir->getXAttr(
         nullptr, PosixACL::defaultACLXAttrName, XATTR_SIZE_MAX);

      if (parentDefaultACLRes == FhgfsOpsErr_SUCCESS)
      {
         // parent has a default ACL
         if (!parentDefaultACL.deserializeXAttr(parentDefaultACLXAttr))
         {
            LogContext(logContext).log(Log_ERR,
               "Error deserializing directory default ACL for directory ID " + parentDir->getID());
            retVal = FhgfsOpsErr_INTERNAL;
            goto clean_up;
         }

         if (!parentDefaultACL.empty())
         {
            // Note: This modifies the mode bits as well as the ACL itself.
            FhgfsOpsErr modeRes = parentDefaultACL.modifyModeBits(mode, needsACL);
            setMode(mode, 0);

            if (modeRes != FhgfsOpsErr_SUCCESS)
            {
               LogContext(logContext).log(Log_ERR, "Error generating access ACL for new directory "
                     + newName);
               retVal = FhgfsOpsErr_INTERNAL;
               goto clean_up;
            }

            if (needsACL)
               parentDefaultACL.serializeXAttr(accessACLXAttr);
         }
         else
         {
            // On empty ACL, clear the Xattr, so it doesn't get set on the newly created dir
            parentDefaultACLXAttr.clear();
         }
      }

      if (parentDefaultACLRes == FhgfsOpsErr_NODATA
            || (parentDefaultACLRes == FhgfsOpsErr_SUCCESS && parentDefaultACL.empty()))
      {
         // containing dir has no ACL, so we can continue without one.
         mode &= ~umask;
         setMode(mode, umask);
      }

      if (parentDefaultACLRes != FhgfsOpsErr_SUCCESS && parentDefaultACLRes != FhgfsOpsErr_NODATA)
      {
         LogContext(logContext).log(Log_ERR,
            "Error loading default ACL for directory ID " + parentDir->getID() );
         retVal = parentDefaultACLRes;
         goto clean_up;
      }
   }

   newEntryInfo.set(NumNodeID(ownerNodeID), parentEntryID, entryID, newName,
      DirEntryType_DIRECTORY, entryInfoFlags);

   // create remote dir metadata
   // (we create this before the dentry to reduce the risk of dangling dentries)
   retVal = mkRemoteDirInode(*parentDir, newName, &newEntryInfo, parentDefaultACLXAttr,
      accessACLXAttr);

   if ( likely(retVal == FhgfsOpsErr_SUCCESS) )
   { // remote dir created => create dentry in parent dir
      // note: we can't lock before this point, because mkRemoteDirInode will also send a message
      // that needs to aquire a lock on the ID
      FileIDLock lock;

      if (parentInfo->getIsBuddyMirrored())
         lock = {entryLockStore, entryID, true};

      retVal = mkDirDentry(*parentDir, newName, &newEntryInfo, isBuddyMirrored);

      if ( retVal != FhgfsOpsErr_SUCCESS )
      { // error (or maybe name just existed already) => compensate metaDir creation
         // note: unlock needs to happen before a possible remoteDirCompensation, because rmDir
         // will also need to lock entryID
         lock = {};
         mkRemoteDirCompensate(&newEntryInfo);
      }
      else
      {
         if (app->getFileEventLogger() && getFileEvent())
         {
            app->getFileEventLogger()->log(
                                          *getFileEvent(),
                                          newEntryInfo.getEntryID(),
                                          newEntryInfo.getParentEntryID());
         }
      }
   }

clean_up:
   metaStore->releaseDir(parentInfo->getEntryID() );

   if (modEventLoggingEnabled)
      modEventFlusher->add(ModificationEvent_DIRCREATED, newEntryInfo.getEntryID());

   return boost::make_unique<ResponseState>(retVal, std::move(newEntryInfo));
}

std::unique_ptr<MkDirMsgEx::ResponseState> MkDirMsgEx::mkDirSecondary()
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   // only create the dentry here; forwarding of inode creation directly happens in MkLocalFileMsg
   // and error handling and compensation is done by primary
   FhgfsOpsErr retVal;

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(getParentInfo()->getEntryID(),
      getParentInfo()->getIsBuddyMirrored(), true);
   if(!parentDir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS, EntryInfo());

   retVal = mkDirDentry(*parentDir, getCreatedEntryInfo()->getFileName(), getCreatedEntryInfo(),
      getCreatedEntryInfo()->getIsBuddyMirrored());

   metaStore->releaseDir(getParentInfo()->getEntryID());

   return boost::make_unique<ResponseState>(retVal, *getCreatedEntryInfo());
}

FhgfsOpsErr MkDirMsgEx::mkDirDentry(DirInode& parentDir, const std::string& name,
   const EntryInfo* entryInfo, const bool isBuddyMirrored)
{
   const std::string entryID     = entryInfo->getEntryID();
   const NumNodeID ownerNodeID    = entryInfo->getOwnerNodeID();

   DirEntry newDirDentry(DirEntryType_DIRECTORY, name, entryID, ownerNodeID);

   if(isBuddyMirrored)
      newDirDentry.setBuddyMirrorFeatureFlag();

   const FhgfsOpsErr mkRes = parentDir.makeDirEntry(newDirDentry);

   if (mkRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(parentDir, parentTimestamps);

   return mkRes;
}

/**
 * Create dir inode on a remote server.
 *
 * @param name only used for logging
 * @param mirrorNodeID 0 for disabled mirroring
 */
FhgfsOpsErr MkDirMsgEx::mkRemoteDirInode(DirInode& parentDir, const std::string& name,
   EntryInfo* entryInfo, const CharVector& defaultACLXAttr, const CharVector& accessACLXAttr)
{
   const char* logContext = "MkDirMsg (mk dir inode)";

   App* app = Program::getApp();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   StripePattern* pattern = parentDir.getStripePatternClone();
   NumNodeID ownerNodeID = entryInfo->getOwnerNodeID();

   LOG_DEBUG(logContext, Log_DEBUG,
      "Creating dir inode at metadata node: " + ownerNodeID.str() + "; dirname: " + name);

   // prepare request

   NumNodeID parentNodeID = app->getLocalNode().getNumID();
   MkLocalDirMsg mkMsg(entryInfo, getUserID(), getGroupID(), getMode(), pattern, parentNodeID,
      defaultACLXAttr, accessACLXAttr);

   RequestResponseArgs rrArgs(NULL, &mkMsg, NETMSGTYPE_MkLocalDirResp);

   RequestResponseNode rrNode(ownerNodeID, app->getMetaNodes() );
   rrNode.setTargetStates(app->getMetaStateStore() );

   if(entryInfo->getIsBuddyMirrored())
      rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);

   do // (this loop just exists to enable the "break"-jump, so it's not really a loop)
   {
      // send request to other mds and receive response

      FhgfsOpsErr requestRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

      if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with metadata server failed. "
            "nodeID: " + ownerNodeID.str() + "; " +
            "dirname: " + name);
         retVal = requestRes;
         break;
      }

      // correct response type received
      const auto mkRespMsg = (const MkLocalDirRespMsg*)rrArgs.outRespMsg.get();

      FhgfsOpsErr mkRemoteInodeRes = mkRespMsg->getResult();
      if(mkRemoteInodeRes != FhgfsOpsErr_SUCCESS)
      { // error: remote dir inode not created
         LogContext(logContext).log(Log_WARNING,
            "Metadata server failed to create dir inode. "
            "nodeID: " + ownerNodeID.str() + "; " +
            "dirname: " + name);

         retVal = mkRemoteInodeRes;
         break;
      }

      // success: remote dir inode created
      LOG_DEBUG(logContext, Log_DEBUG,
         "Metadata server created dir inode. "
         "nodeID: " + ownerNodeID.str() + "; "
         "dirname: " + name);

   } while(false);


   delete(pattern);

   return retVal;
}

/**
 * Remove dir metadata on a remote server to compensate for creation.
 */
FhgfsOpsErr MkDirMsgEx::mkRemoteDirCompensate(EntryInfo* entryInfo)
{
   LogContext log("MkDirMsg (undo dir inode [" + entryInfo->getFileName() + "])");

   FhgfsOpsErr rmRes = RmDirMsgEx::rmRemoteDirInode(entryInfo);

   if(unlikely(rmRes != FhgfsOpsErr_SUCCESS) )
   { // error
      log.log(Log_WARNING, std::string("Compensation not completely successful. ") +
         "File system might contain (uncritical) inconsistencies.");

      return rmRes;
   }

   log.log(Log_SPAM, "Creation of dir inode compensated");

   return rmRes;
}

void MkDirMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   // secondary needs to know the created entryInfo, because it needs to use the same information
   setCreatedEntryInfo(&newEntryInfo);

   // not needed on secondary
   clearPreferredNodes();

   sendToSecondary(ctx, *this, NETMSGTYPE_MkDirResp);
}
