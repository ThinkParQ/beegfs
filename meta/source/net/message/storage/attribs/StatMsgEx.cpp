#include <program/Program.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperStat.h>
#include "StatMsgEx.h"


bool StatMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "StatMsgEx incoming";
#endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, 5, "ParentID: " + getEntryInfo()->getParentEntryID() +
      "; EntryID: " + getEntryInfo()->getEntryID() +
      "; EntryType: " + StringTk::intToStr(getEntryInfo()->getEntryType() ) +
      "; isBuddyMirrored: " + StringTk::intToStr(getEntryInfo()->getIsBuddyMirrored()));

   return BaseType::processIncoming(ctx);
}

FileIDLock StatMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), false};
}

std::unique_ptr<MirroredMessageResponseState> StatMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   StatMsgResponseState resp;

   StatData statData;
   FhgfsOpsErr statRes;

   NumNodeID parentNodeID;
   std::string parentEntryID;

   EntryInfo* entryInfo = this->getEntryInfo();

   if (entryInfo->getParentEntryID().empty() || (entryInfo->getEntryID() == META_ROOTDIR_ID_STR) )
   { // special case: stat for root directory
      statRes = statRoot(statData);
   }
   else
   {
      statRes = MsgHelperStat::stat(entryInfo, true, getMsgHeaderUserID(), statData, &parentNodeID,
         &parentEntryID);
   }

   LOG_DBG(GENERAL, DEBUG, "", statRes);

   resp.setStatResult(statRes);
   resp.setStatData(statData);

   if (isMsgHeaderFeatureFlagSet(STATMSG_FLAG_GET_PARENTINFO) && parentNodeID)
      resp.setParentInfo(parentNodeID, parentEntryID);

   updateNodeOp(ctx, MetaOpCounter_STAT);

   return boost::make_unique<ResponseState>(std::move(resp));
}

FhgfsOpsErr StatMsgEx::statRoot(StatData& outStatData)
{
   App* app = Program::getApp();
   Node& localNode = app->getLocalNode();
   DirInode* rootDir = app->getRootDir();
   NumNodeID expectedOwnerID;

   // if root is buddy mirrored compare ownership to buddy group id, otherwise to node id itself
   if ( rootDir->getIsBuddyMirrored() )
      expectedOwnerID =
         NumNodeID(app->getMetaBuddyGroupMapper()->getBuddyGroupID(localNode.getNumID().val() ) );
   else
      expectedOwnerID = localNode.getNumID();

   if(expectedOwnerID != rootDir->getOwnerNodeID() )
   {
      return FhgfsOpsErr_NOTOWNER;
   }

   rootDir->getStatData(outStatData);


   return FhgfsOpsErr_SUCCESS;
}

