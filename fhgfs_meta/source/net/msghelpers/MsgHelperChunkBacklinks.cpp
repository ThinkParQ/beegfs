#include "MsgHelperChunkBacklinks.h"

#include <common/net/message/storage/attribs/UpdateBacklinkMsg.h>
#include <common/net/message/storage/attribs/UpdateBacklinkRespMsg.h>
#include <program/Program.h>
#include <storage/MetaStore.h>

FhgfsOpsErr MsgHelperChunkBacklinks::updateBacklink(const std::string& parentID,
   bool isParentBuddyMirrored, const std::string& name)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   DirInode* parentDirInode = metaStore->referenceDir(parentID, isParentBuddyMirrored, true);

   FhgfsOpsErr retVal = updateBacklink(*parentDirInode, name);

   metaStore->releaseDir(parentID);

   return retVal;
}

FhgfsOpsErr MsgHelperChunkBacklinks::updateBacklink(DirInode& parent, const std::string& name)
{
   EntryInfo entryInfo;
   parent.getEntryInfo(name, entryInfo);

   return updateBacklink(entryInfo);
}

/**
 * Note: this method works for buddymirrored files, but uses only group's primary.
 */
FhgfsOpsErr MsgHelperChunkBacklinks::updateBacklink(EntryInfo& entryInfo)
{
   const char* logContext = "MsgHelperChunkBacklinks (update backlinks)";

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   MetaFileHandle fileInode = metaStore->referenceFile(&entryInfo);

   if (!fileInode)
   {
      LogContext(logContext).logErr("Unable to reference file with ID: " + entryInfo.getEntryID() );
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   PathInfo pathInfo;
   fileInode->getPathInfo(&pathInfo);

   StripePattern* pattern = fileInode->getStripePattern();
   const UInt16Vector* stripeTargets = pattern->getStripeTargetIDs();

   // send update msg to each stripe target
   for ( UInt16VectorConstIter iter = stripeTargets->begin(); iter != stripeTargets->end(); iter++ )
   {
      UpdateBacklinkRespMsg* updateBacklinkRespMsg;
      FhgfsOpsErr updateBacklinkRes;

      uint16_t targetID = *iter;

      // prepare request message

      boost::scoped_array<char> serialEntryInfoBuf;
      ssize_t serialEntryInfoBufLen = serializeIntoNewBuffer(entryInfo, serialEntryInfoBuf);

      if(unlikely(serialEntryInfoBufLen < 0) )
      { // malloc failed
         LogContext(logContext).log(Log_CRITICAL,
            std::string("Memory allocation for backlink buffer failed.") +
            (pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
            "TargetID: " + StringTk::uintToStr(targetID) + "; "
            "EntryID: " + entryInfo.getEntryID() );

         continue;
      }

      std::string entryID(entryInfo.getEntryID() ); // copy needed for UpdateBacklinkMsg constructor

      UpdateBacklinkMsg updateBacklinkMsg(entryID, targetID, &pathInfo,
         serialEntryInfoBuf.get(), serialEntryInfoBufLen);

      if(pattern->getPatternType() == StripePatternType_BuddyMirror)
         updateBacklinkMsg.addMsgHeaderFeatureFlag(UPDATEBACKLINKSMSG_FLAG_BUDDYMIRROR);

      // prepare communication

      RequestResponseTarget rrTarget(targetID, app->getTargetMapper(), app->getStorageNodes() );

      rrTarget.setTargetStates(app->getTargetStateStore() );

      if(pattern->getPatternType() == StripePatternType_BuddyMirror)
         rrTarget.setMirrorInfo(app->getStorageBuddyGroupMapper(), false);

      RequestResponseArgs rrArgs(NULL, &updateBacklinkMsg, NETMSGTYPE_UpdateBacklinkResp);

      // communicate

      FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

      if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            std::string("Communication with storage target failed. ") +
            (pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
            "TargetID: " + StringTk::uintToStr(targetID) + "; "
            "EntryID: " + entryID);
         retVal = FhgfsOpsErr_INTERNAL;
         continue;
      }

      updateBacklinkRespMsg = (UpdateBacklinkRespMsg*)rrArgs.outRespMsg;
      updateBacklinkRes = (FhgfsOpsErr)updateBacklinkRespMsg->getValue();

      if(updateBacklinkRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).logErr(
            std::string("Updating backlink on target failed. ") +
            (pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
            "TargetID: " + StringTk::uintToStr(targetID) + "; "
            "EntryID: " + entryID + "; "
            "Error: " + std::string(FhgfsOpsErrTk::toErrString(updateBacklinkRes) ) );
         retVal = FhgfsOpsErr_INTERNAL;
      }
   }

   metaStore->releaseFile(entryInfo.getParentEntryID(), fileInode);

   return retVal;
}
