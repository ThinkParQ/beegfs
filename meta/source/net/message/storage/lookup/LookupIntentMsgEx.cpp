#include <common/net/message/control/GenericResponseMsg.h>
#include <program/Program.h>
#include <common/net/message/storage/creating/MkFileMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/SessionTk.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <net/msghelpers/MsgHelperOpen.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <session/EntryLock.h>
#include <session/SessionStore.h>
#include <storage/DentryStoreData.h>
#include <common/storage/StoragePool.h>
#include "LookupIntentMsgEx.h"


std::tuple<FileIDLock, ParentNameLock, FileIDLock> LookupIntentMsgEx::lock(EntryLockStore& store)
{
   ParentNameLock dentryLock;
   FileIDLock entryIDLockForDir;
   FileIDLock entryIDLockForFile;
   enum DirEntryType entryType = getEntryInfo()->getEntryType();

   if (getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE)
   {
      entryIDLockForDir = {&store, getParentInfo()->getEntryID(), true};
      dentryLock = {&store, getParentInfo()->getEntryID(), getEntryName()};
      entryIDLockForFile = {&store, entryID, true};
   }
   else
   {
      //XXX Note: If you are addding new flag for lookupintent, make sure to
      //check if shared lock for parent dir and file is sufficient.
      //Otherwise add another else if condition.
      // For other lookup flag, we don't have entryID yet. So rather than
      // taking lock on the emptry EntryID(which can serialize lookup on all
      // files even if from different parent directory), lookup the entryID
      // while holding parent lock. And take the lock on the actual entryID.
      entryIDLockForDir = {&store, getParentInfo()->getEntryID(), false};

      lookupRes = lookup(getParentInfo()->getEntryID(), getEntryName(), isMirrored(),
            &diskEntryInfo, &inodeData, inodeDataOutdated);
      if (lookupRes == FhgfsOpsErr_SUCCESS)
      {
         entryID = diskEntryInfo.getEntryID();
         entryType = diskEntryInfo.getEntryType();
      }

      // for revalidate entryInfo is valid, so use the info from it
      if (entryID.empty() && (getIntentFlags() & LOOKUPINTENTMSG_FLAG_REVALIDATE))
      {
         entryID =  getEntryInfo()->getEntryID();
         entryType =  getEntryInfo()->getEntryType();
      }

      if (getIntentFlags() & LOOKUPINTENTMSG_FLAG_OPEN)
      {
         entryIDLockForFile = {&store, entryID, true};
      }
      else
      {
         if (DirEntryType_ISDIR(entryType) &&
               (entryID < getParentInfo()->getEntryID()))
         {
            // Release parent lock because child lock needs to be taken first.
            entryIDLockForDir = {};

            // Take the lock in reverse order
            entryIDLockForFile = {&store, entryID, false};
            entryIDLockForDir = {&store, getParentInfo()->getEntryID(), false};
         }
         else
         {
            entryIDLockForFile = {&store, entryID, false};
         }
      }
   }

   return std::make_tuple(std::move(entryIDLockForDir), std::move(dentryLock),
                          std::move(entryIDLockForFile));
}

bool LookupIntentMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   const std::string& parentEntryID = getParentInfo()->getEntryID();
   const std::string& entryName = getEntryName();

   lookupRes = FhgfsOpsErr_INTERNAL;

   inodeDataOutdated = false; // true if the file/inode is currently open (referenced)

   LOG_DBG(GENERAL, DEBUG, "", parentEntryID, entryName, getParentInfo()->getIsBuddyMirrored());

   // sanity checks
   if (unlikely(parentEntryID.empty() || entryName.empty()))
   {
      LOG(GENERAL, WARNING, "Sanity check failed", parentEntryID, entryName);

      ctx.sendResponse(LookupIntentRespMsg(FhgfsOpsErr_INTERNAL));
      return true;
   }

   if (getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE)
      entryID = StorageTk::generateFileID(app->getLocalNode().getNumID());

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> LookupIntentMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   LookupIntentResponseState response;

   int createFlag = getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE;
   FhgfsOpsErr createRes = FhgfsOpsErr_INTERNAL;

   // Note: Actually we should first do a lookup. However, a successful create also implies a
   // failed Lookup, so we can take a shortcut.

   // lookup-create
   if (createFlag)
   {
      LOG_DBG(GENERAL, SPAM, "Create");

      createRes = create(getParentInfo(), getEntryName(), &diskEntryInfo, &inodeData, isSecondary);
      switch (createRes)
      {
         case FhgfsOpsErr_SUCCESS:
            // Successful Create, which implies Lookup-before-create would have been failed.
            response.setLookupResult(FhgfsOpsErr_PATHNOTEXISTS);
            response.setEntryInfo(diskEntryInfo);
            response.addResponseCreate(createRes);
            break;

         case FhgfsOpsErr_EXISTS:
            // NOTE: we need to do a Lookup to get required lookup data
            if (getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE)
               response.addResponseCreate(FhgfsOpsErr_EXISTS);
            else
               response.addResponseCreate(FhgfsOpsErr_SUCCESS);
            break;
         case FhgfsOpsErr_DQUOT:
            response.addResponseCreate(FhgfsOpsErr_DQUOT);
            break;

         default:
            response.addResponseCreate(FhgfsOpsErr_INTERNAL);
      }

      // note: don't quit here on error because caller might still have requested stat info
   }

   // lookup
   if ((!createFlag) || (createRes == FhgfsOpsErr_EXISTS) )
   {
      LOG_DBG(GENERAL, SPAM, "Lookup");

      // Lookup will happen for secondary node or create failed or lookup
      // failed earlier.
      if (isSecondary || (createRes == FhgfsOpsErr_EXISTS) ||
          lookupRes != FhgfsOpsErr_SUCCESS)
      {
         // lock is not taken on secondary, so we need to perform lookup()
         // for secondary here.
          lookupRes = lookup(getParentInfo()->getEntryID(), getEntryName(), isMirrored(),
                 &diskEntryInfo, &inodeData, inodeDataOutdated);

      }
      // Lookup was already done in lock() function after taking lock on parent.
      response.setLookupResult(lookupRes);
      response.setEntryInfo(diskEntryInfo);

      if (unlikely(lookupRes != FhgfsOpsErr_SUCCESS && createFlag))
      {
         // so createFlag is set, so createRes is either Success or Exists, but now lookup fails
         // create/unlink race?

         StatData statData;
         statData.setAllFake(); // set arbitrary stat values (receiver won't use the values)

         response.addResponseStat(lookupRes, statData);

         return boost::make_unique<ResponseState>(std::move(response));
      }
   }

   // lookup-revalidate
   if (getIntentFlags() & LOOKUPINTENTMSG_FLAG_REVALIDATE)
   {
      LOG_DBG(GENERAL, SPAM, "Revalidate");

      auto revalidateRes = revalidate(&diskEntryInfo);
      response.addResponseRevalidate(revalidateRes);

      if (revalidateRes != FhgfsOpsErr_SUCCESS)
         return boost::make_unique<ResponseState>(std::move(response));
   }

   // lookup-stat
   // note: we do stat before open to avoid the dyn attribs refresh if the file is not opened by
   // someone else currently.
   if ((getIntentFlags() & LOOKUPINTENTMSG_FLAG_STAT) &&
        (lookupRes == FhgfsOpsErr_SUCCESS || createRes == FhgfsOpsErr_SUCCESS))
   {
      LOG_DBG(GENERAL, SPAM, "Stat");

      // check if lookup and create failed (we don't have an entryID to stat then)
      if (diskEntryInfo.getEntryID().empty())
         return boost::make_unique<ResponseState>(std::move(response));

      if ((diskEntryInfo.getFeatureFlags() & ENTRYINFO_FEATURE_INLINED) && !inodeDataOutdated)
      {
         // For inlined inodes with up-to-date inode data, use stat data fetched during lookup()
         response.addResponseStat(FhgfsOpsErr_SUCCESS, *inodeData.getInodeStatData());
      }
      else
      {  // read stat data separately
         StatData statData;
         FhgfsOpsErr statRes = stat(&diskEntryInfo, true, statData);

         response.addResponseStat(statRes, statData);

         if (statRes != FhgfsOpsErr_SUCCESS)
            return boost::make_unique<ResponseState>(std::move(response));
      }
   }

   // lookup-open
   if(getIntentFlags() & LOOKUPINTENTMSG_FLAG_OPEN)
   {
      LOG_DBG(GENERAL, SPAM, "Open");

      std::string fileHandleID;
      PathInfo pathInfo;
      Raid0Pattern dummyPattern(1, UInt16Vector() );

      // don't open if create failed
      if ((createRes != FhgfsOpsErr_SUCCESS) && (getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE))
         return boost::make_unique<ResponseState>(std::move(response));

      // if it's not a regular file, we don't open that
      if (!DirEntryType_ISREGULARFILE(diskEntryInfo.getEntryType()))
         return boost::make_unique<ResponseState>(std::move(response));

      // check if lookup and/or create failed (we don't have an entryID to open then)
      if (diskEntryInfo.getEntryID().empty())
         return boost::make_unique<ResponseState>(std::move(response));

      StripePattern* pattern = NULL;

      FhgfsOpsErr openRes = open(&diskEntryInfo, &fileHandleID, &pattern, &pathInfo, isSecondary);

      if(openRes != FhgfsOpsErr_SUCCESS)
      { // open failed => use dummy pattern for response
         response.addResponseOpen(openRes, std::move(fileHandleID),
               std::unique_ptr<StripePattern>(dummyPattern.clone()), pathInfo);

         return boost::make_unique<ResponseState>(std::move(response));
      }

      response.addResponseOpen(openRes, std::move(fileHandleID),
            std::unique_ptr<StripePattern>(pattern->clone()), pathInfo);
   }

   updateNodeOp(ctx, getOpCounterType());

   return boost::make_unique<ResponseState>(std::move(response));
}

FhgfsOpsErr LookupIntentMsgEx::lookup(const std::string& parentEntryID,
   const std::string& entryName, bool isBuddyMirrored, EntryInfo* outEntryInfo,
   FileInodeStoreData* outInodeStoreData, bool& outInodeDataOutdated)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   DirInode* parentDir = metaStore->referenceDir(parentEntryID, isBuddyMirrored, false);
   if (unlikely(!parentDir))
   { // out of memory
      return FhgfsOpsErr_INTERNAL;
   }

   auto [lookupRes1, isFileOpen] = metaStore->getEntryData(parentDir, entryName, outEntryInfo,
      outInodeStoreData);

   if (lookupRes1 == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED)
   {
      lookupRes1 = FhgfsOpsErr_SUCCESS;
      outInodeDataOutdated = isFileOpen;

      // If the file inode is not inlined and the intent includes the creation flag,
      // we need to use a different overload of getEntryData() to correctly retrieve the
      // inode disk data for non-inlined inode(s) and prevent potential crashes due to
      // race conditions between create and hardlink creation.
      // If create flag is set, fetch full inode disk data using non-inlined inode.
      int createFlag = getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE;
      if (!outEntryInfo->getIsInlined() && createFlag)
      {
         FhgfsOpsErr getRes = metaStore->getEntryData(outEntryInfo, outInodeStoreData);
         if (getRes != FhgfsOpsErr_SUCCESS)
            lookupRes1 = getRes;
      }
   }

   metaStore->releaseDir(parentEntryID);
   return lookupRes1;
}

/**
 * compare entryInfo on disk with EntryInfo send by the client
 * @return FhgfsOpsErr_SUCCESS if revalidation successful, FhgfsOpsErr_PATHNOTEXISTS otherwise.
 */
FhgfsOpsErr LookupIntentMsgEx::revalidate(EntryInfo* diskEntryInfo)
{
   EntryInfo* clientEntryInfo = this->getEntryInfo();

   if ( (diskEntryInfo->getEntryID()     == clientEntryInfo->getEntryID() ) &&
        (diskEntryInfo->getOwnerNodeID() == clientEntryInfo->getOwnerNodeID() ) )
      return FhgfsOpsErr_SUCCESS;


   return FhgfsOpsErr_PATHNOTEXISTS;
}

FhgfsOpsErr LookupIntentMsgEx::create(EntryInfo* parentInfo, const std::string& entryName,
   EntryInfo *outEntryInfo, FileInodeStoreData* outInodeData, bool isSecondary)
{
   MkFileDetails mkDetails(entryName, getUserID(), getGroupID(), getMode(), getUmask(),
         TimeAbs().getTimeval()->tv_sec);
   StripePattern* pattern = nullptr;
   FhgfsOpsErr res;

   DirInode* dir = Program::getApp()->getMetaStore()->referenceDir(
         parentInfo->getEntryID(), parentInfo->getIsBuddyMirrored(), true);
   if (!dir)
      return FhgfsOpsErr_PATHNOTEXISTS;

   if (isSecondary)
   {
      mkDetails.setNewEntryID(getNewEntryID().c_str());
      mkDetails.createTime = fileTimestamps.creationTimeSecs;
      pattern = getNewStripePattern()->clone();
   }
   else
   {
      if (!entryID.empty())
         mkDetails.setNewEntryID(entryID.c_str());

      pattern = dir->createFileStripePattern(&getPreferredTargets(), 0, 0, StoragePoolId(0));
   }

   if (!pattern)
   {
      LogContext("Lookup Create").logErr(
         "StripePattern is NULL. Can't proceed. Filename: " + getEntryName());

      res = FhgfsOpsErr_INTERNAL;
      goto releasedir_and_return;
   }

   if (isMsgHeaderFeatureFlagSet(LOOKUPINTENTMSG_FLAG_USE_QUOTA) &&
         Program::getApp()->getConfig()->getQuotaEnableEnforcement())
   {
      const char* logContext = "Quota Enforcement for create";
      StoragePoolPtr storagePool =
         Program::getApp()->getStoragePoolStore()->getPool(pattern->getStoragePoolId());

      if (!storagePool)
      {
         LOG(QUOTA, WARNING, "Requested storage pool doesn't exist on metadata server.",
                      ("StoragePoolId", pattern->getStoragePoolId()));

         res = FhgfsOpsErr_UNKNOWNPOOL;
         goto releasedir_and_return;
      }

      UInt16Set targetIds = storagePool->getTargets();
      for (auto targetId : targetIds)
      {
         ExceededQuotaStorePtr exceededQuotaStore = Program::getApp()->getExceededQuotaStores()->get(targetId);
         if (exceededQuotaStore && exceededQuotaStore->someQuotaExceeded())
         {
            QuotaExceededErrorType quotaExceeded = exceededQuotaStore->isQuotaExceeded(mkDetails.userID, mkDetails.groupID);
            if (quotaExceeded != QuotaExceededErrorType_NOT_EXCEEDED)
            {
               LogContext(logContext).log(Log_NOTICE,
                  QuotaData::QuotaExceededErrorTypeToString(quotaExceeded) + " "
                  "UID: " + StringTk::uintToStr(mkDetails.userID) + "; "
                  "GID: " + StringTk::uintToStr(mkDetails.groupID));
               res = FhgfsOpsErr_DQUOT;
               goto releasedir_and_return;
            }
         }
      }
   }

   res = MsgHelperMkFile::mkFile(*dir, &mkDetails, &getPreferredTargets(), 0, 0,
      pattern, outEntryInfo, outInodeData);

   if (res == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      fixInodeTimestamp(*dir, dirTimestamps);

      if (!isSecondary)
         fileTimestamps = outInodeData->getInodeStatData()->getMirroredTimestamps();
   }

   if (!isSecondary && res == FhgfsOpsErr_SUCCESS && Program::getApp()->getFileEventLogger()
       && getFileEvent())
   {
         Program::getApp()->getFileEventLogger()->log(
                  *getFileEvent(),
                  outEntryInfo->getEntryID(),
                  outEntryInfo->getParentEntryID());
   }

releasedir_and_return:
   Program::getApp()->getMetaStore()->releaseDir(dir->getID());

   return res;
}

FhgfsOpsErr LookupIntentMsgEx::stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData)
{
   Node& localNode = Program::getApp()->getLocalNode();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = Program::getApp()->getMetaBuddyGroupMapper();

   FhgfsOpsErr statRes = FhgfsOpsErr_NOTOWNER;

   // check if we can stat on this machine or if entry is owned by another server
   NumNodeID expectedOwner;
   if (entryInfo->getIsBuddyMirrored())
      expectedOwner = NumNodeID(metaBuddyGroupMapper->getLocalGroupID());
   else
      expectedOwner = localNode.getNumID();

   if(entryInfo->getOwnerNodeID() == expectedOwner)
      statRes = MsgHelperStat::stat(entryInfo, loadFromDisk, getMsgHeaderUserID(), outStatData);

   return statRes;
}

/**
 * @param outPattern only set if success is returned; points to referenced open file, so it does
 * not need to be free'd/deleted.
 */
FhgfsOpsErr LookupIntentMsgEx::open(EntryInfo* entryInfo, std::string* outFileHandleID,
   StripePattern** outPattern, PathInfo* outPathInfo, bool isSecondary)
{
   App* app = Program::getApp();
   SessionStore* sessions = entryInfo->getIsBuddyMirrored()
      ? app->getMirroredSessions()
      : app->getSessions();

   MetaFileHandle inode;

   bool useQuota = isMsgHeaderFeatureFlagSet(LOOKUPINTENTMSG_FLAG_USE_QUOTA);

   FhgfsOpsErr openRes = MsgHelperOpen::openFile(
      entryInfo, getAccessFlags(), useQuota, getMsgHeaderUserID(), inode,
      isSecondary);

   if (openRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(*inode, fileTimestamps, entryInfo);

   if(openRes != FhgfsOpsErr_SUCCESS)
      return openRes; // error occurred

   // open successful => insert session

   SessionFile* sessionFile = new SessionFile(std::move(inode), getAccessFlags(), entryInfo);

   Session* session = sessions->referenceSession(getSessionID(), true);

   *outPattern = sessionFile->getInode()->getStripePattern();
   sessionFile->getInode()->getPathInfo(outPathInfo);

   unsigned sessionFileID;

   if (!hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
   {
      sessionFileID = session->getFiles()->addSession(sessionFile);
      newOwnerFD = sessionFileID;
   }
   else
   {
      sessionFileID = newOwnerFD;
      bool addRes = session->getFiles()->addSession(sessionFile, sessionFileID);

      if (!addRes)
         LOG(GENERAL, NOTICE, "Couldn't add sessionFile on secondary",
            ("sessionID", this->getSessionID()), sessionFileID);
   }

   sessions->releaseSession(session);

   *outFileHandleID = SessionTk::generateFileHandleID(sessionFileID, entryInfo->getEntryID() );

   return openRes;
}

/**
 * Decide which type of client stats op counter we increase for this msg (based on given msg flags).
 */
MetaOpCounterTypes LookupIntentMsgEx::getOpCounterType()
{
   /* note: as introducting a speparate opCounter type for each flag would have been too much,
      we assign prioritiess here as follows: create > open > revalidate > stat > simple_no_flags */

   /* NOTE: Those if's are rather slow, maybe we should create a table that has the values?
    *       Problem is that the table has to be filled for flag combinations, which is also ugly
    */

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE)
      return MetaOpCounter_LOOKUPINTENT_CREATE;

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_OPEN)
      return MetaOpCounter_LOOKUPINTENT_OPEN;

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_REVALIDATE)
      return MetaOpCounter_LOOKUPINTENT_REVALIDATE;

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_STAT)
      return MetaOpCounter_LOOKUPINTENT_STAT;

   return MetaOpCounter_LOOKUPINTENT_SIMPLE;

}

void LookupIntentMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   addBuddyInfo(entryID, inodeData.getStripePattern());
   sendToSecondary(ctx, *this, NETMSGTYPE_LookupIntentResp);
}
