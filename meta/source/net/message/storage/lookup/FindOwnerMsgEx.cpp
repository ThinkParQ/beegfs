#include <program/Program.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include "FindOwnerMsgEx.h"


bool FindOwnerMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "FindOwnerMsg incoming";
   #endif // BEEGFS_DEBUG
   
   LOG_DEBUG(logContext, Log_SPAM, "Path: '" + getPath().str() + "'");

   EntryInfoWithDepth ownerInfo;
   FhgfsOpsErr findRes;
   
   if(!getSearchDepth() )
   { // looking for the root node
      NumNodeID rootNodeID = Program::getApp()->getRootDir()->getOwnerNodeID();
      
      if (rootNodeID)
      {
         ownerInfo.set(rootNodeID, "", META_ROOTDIR_ID_STR, "/", DirEntryType_DIRECTORY, 0, 0);
         
         if (Program::getApp()->getRootDir()->getIsBuddyMirrored())
            ownerInfo.setBuddyMirroredFlag(true);

         findRes = FhgfsOpsErr_SUCCESS;
      }
      else
      { // we don't know the root node
         findRes = FhgfsOpsErr_INTERNAL;
      }
   }
   else
   { // a normal path lookup
      findRes = findOwner(&ownerInfo);
   }

   ctx.sendResponse(FindOwnerRespMsg(findRes, &ownerInfo) );

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_FINDOWNER,
      getMsgHeaderUserID() );

   return true;
}


/**
 * Note: This does not work for finding the root dir owner (because it relies on the existence
 * of a parent dir).
 */
FhgfsOpsErr FindOwnerMsgEx::findOwner(EntryInfoWithDepth* outInfo)
{
   FhgfsOpsErr findRes = FhgfsOpsErr_INTERNAL;

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   unsigned searchDepth = this->getSearchDepth();
   unsigned currentDepth = this->getCurrentDepth();
   EntryInfo *currentEntryInfo = this->getEntryInfo();
   std::string currentEntryID = currentEntryInfo->getEntryID();
   bool currentEntryIsBuddyMirrored = currentEntryInfo->getIsBuddyMirrored();

   const Path path(getPath());

   // search
   while(currentDepth < searchDepth)
   {
      DirInode* currentDir = metaStore->referenceDir(currentEntryID, currentEntryIsBuddyMirrored,
         true);
      if(!currentDir)
      { // someone said we own the dir with this ID, but we do not => it was deleted
         // note: this might also be caused by a change of ownership, but a feature like that is
         // currently not implemented
         return (findRes == FhgfsOpsErr_SUCCESS) ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_PATHNOTEXISTS;
      }

      EntryInfo subInfo;
      auto [getEntryRes, isFileOpen] = metaStore->getEntryData(currentDir, path[currentDepth],
         &subInfo, NULL);

      if (getEntryRes == FhgfsOpsErr_SUCCESS || getEntryRes == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED)
      { // next entry exists and is a dir or a file
         currentDepth++;

         *outInfo = subInfo;
         outInfo->setEntryDepth(currentDepth);

         findRes = FhgfsOpsErr_SUCCESS;
      }
      else
      { // entry does not exist
         findRes = FhgfsOpsErr_PATHNOTEXISTS;
      }

      metaStore->releaseDir(currentEntryID);

      if(findRes != FhgfsOpsErr_SUCCESS)
         break;

      // is next entry owned by a different node?
      if (subInfo.getIsBuddyMirrored())
      {
         if(subInfo.getOwnerNodeID()
               != NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() ) )
            break;
      }
      else
      if (subInfo.getOwnerNodeID() != app->getLocalNode().getNumID())
         break;

      // prepare for next round
      currentEntryID = outInfo->getEntryID();
   }

   return findRes;
}
