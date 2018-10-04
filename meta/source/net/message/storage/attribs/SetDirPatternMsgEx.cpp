#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include <storage/DirInode.h>
#include <storage/MetaStore.h>
#include "SetDirPatternMsgEx.h"


bool SetDirPatternMsgEx::processIncoming(ResponseContext& ctx)
{
   EntryInfo* entryInfo = this->getEntryInfo();

   LOG_DEBUG("SetDirPatternMsgEx::processIncoming", Log_SPAM,
      "parentEntryID: " + entryInfo->getParentEntryID() + " EntryID: " +
      entryInfo->getEntryID() + " BuddyMirrored: " +
      (entryInfo->getIsBuddyMirrored() ? "Yes" : "No") + " Secondary: " +
      (hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ? "Yes" : "No"));
   (void) entryInfo;

   BaseType::processIncoming(ctx);

   updateNodeOp(ctx, MetaOpCounter_SETDIRPATTERN);

   return true;
}


std::unique_ptr<MirroredMessageResponseState> SetDirPatternMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   const char* logContext = "SetDirPatternMsg (set dir pattern)";

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();


   EntryInfo* entryInfo = getEntryInfo();
   StripePattern* pattern = &getPattern();

   FhgfsOpsErr retVal = FhgfsOpsErr_NOTADIR;

   uint32_t actorUID = isMsgHeaderFeatureFlagSet(Flags::HAS_UID)
      ? getUID()
      : 0;

   if (actorUID != 0 && !app->getConfig()->getSysAllowUserSetPattern())
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PERM);

   if (pattern->getChunkSize() < STRIPEPATTERN_MIN_CHUNKSIZE ||
       !MathTk::isPowerOfTwo(pattern->getChunkSize()))
   { // check of stripe pattern details validity failed
      LOG(GENERAL, ERR, "Received an invalid pattern chunksize",
          ("chunkSize", pattern->getChunkSize()));
      return boost::make_unique<ResponseState>(FhgfsOpsErr_INTERNAL);
   }

   // verify owner of root dir
   if (entryInfo->getEntryID() == META_ROOTDIR_ID_STR)
   {
      const bool isMirrored = entryInfo->getIsBuddyMirrored();
      const NumNodeID rootOwnerID = app->getRootDir()->getOwnerNodeID();
      const NumNodeID localGroupID(app->getMetaBuddyGroupMapper()->getLocalGroupID());

      if ((!isMirrored && rootOwnerID != app->getLocalNodeNumID())
            || (isMirrored && rootOwnerID != localGroupID))
      {
         LogContext(logContext).log(Log_DEBUG, "This node does not own the root directory.");
         return boost::make_unique<ResponseState>(FhgfsOpsErr_NOTOWNER);
      }
   }

   DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored(),
      true);
   if(dir)
   { // entry is a directory
      retVal = dir->setStripePattern(*pattern, actorUID);
      if (retVal != FhgfsOpsErr_SUCCESS)
         LogContext(logContext).logErr("Update of stripe pattern failed. "
            "DirID: " + entryInfo->getEntryID() );

      metaStore->releaseDir(entryInfo->getEntryID() );
   }

   return boost::make_unique<ResponseState>(retVal);
}

void SetDirPatternMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_SetDirPatternResp);
}
