#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include "GetEntryInfoMsgEx.h"


bool GetEntryInfoMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "GetEntryInfoMsg incoming";
#endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_SPAM,
      "ParentEntryID: " + this->getEntryInfo()->getParentEntryID() + "; "
      "entryID: " + this->getEntryInfo()->getParentEntryID() );

   return BaseType::processIncoming(ctx);
}

FileIDLock GetEntryInfoMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), false};
}

std::unique_ptr<MirroredMessageResponseState> GetEntryInfoMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   GetEntryInfoMsgResponseState resp;

   EntryInfo* entryInfo = this->getEntryInfo();
   StripePattern* pattern = NULL;
   FhgfsOpsErr getInfoRes;
   PathInfo pathInfo;

   if (entryInfo->getParentEntryID().empty())
   {
      // special case: get info for root directory
      // no pathInfo here, as this is currently only used for fileInodes
      getInfoRes = getRootInfo(&pattern);
   }
   else
   {
      getInfoRes = getInfo(entryInfo, &pattern, &pathInfo);
   }


   if (getInfoRes != FhgfsOpsErr_SUCCESS)
   { // error occurred => create a dummy pattern
      UInt16Vector dummyStripeNodes;
      pattern = new Raid0Pattern(1, dummyStripeNodes);
   }

   resp.setGetEntryInfoResult(getInfoRes);
   resp.setStripePattern(pattern);
   resp.setPathInfo(pathInfo);

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_GETENTRYINFO,
      getMsgHeaderUserID() );

   return boost::make_unique<ResponseState>(std::move(resp));
}


/**
 * @param outPattern StripePattern clone (in case of success), that must be deleted by the caller
 */
FhgfsOpsErr GetEntryInfoMsgEx::getInfo(EntryInfo* entryInfo, StripePattern** outPattern,
   PathInfo* outPathInfo)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   if (entryInfo->getEntryType() == DirEntryType_DIRECTORY)
   { // entry is a directory
      DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(),
         entryInfo->getIsBuddyMirrored(), true);
      if(dir)
      {
         *outPattern = dir->getStripePatternClone();

         metaStore->releaseDir(entryInfo->getEntryID() );
         return FhgfsOpsErr_SUCCESS;
      }
   }
   else
   { // entry is a file
      MetaFileHandle fileInode = metaStore->referenceFile(entryInfo);
      if(fileInode)
      {
         *outPattern = fileInode->getStripePattern()->clone();
         fileInode->getPathInfo(outPathInfo);

         metaStore->releaseFile(entryInfo->getParentEntryID(), fileInode);
         return FhgfsOpsErr_SUCCESS;
      }
   }

   return FhgfsOpsErr_PATHNOTEXISTS;
}

/**
 * @param outPattern StripePattern clone (in case of success), that must be deleted by the caller
 */
FhgfsOpsErr GetEntryInfoMsgEx::getRootInfo(StripePattern** outPattern)
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();
   DirInode* rootDir = app->getRootDir();

   NumNodeID localNodeID = app->getLocalNodeNumID();

   if (rootDir->getIsBuddyMirrored())
   {
      uint16_t buddyGroupID = metaBuddyGroupMapper->getBuddyGroupID(localNodeID.val());
      if (buddyGroupID != rootDir->getOwnerNodeID().val() )
         return FhgfsOpsErr_NOTOWNER;
   }
   else
   if (localNodeID != rootDir->getOwnerNodeID() )
      return FhgfsOpsErr_NOTOWNER;

   *outPattern = rootDir->getStripePatternClone();

   return FhgfsOpsErr_SUCCESS;
}
