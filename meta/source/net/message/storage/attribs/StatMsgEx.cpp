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

   App* app = Program::getApp();
   EntryInfo* entryInfo = this->getEntryInfo();

   LOG_DEBUG(logContext, 5, "ParentID: " + entryInfo->getParentEntryID() +
      "; EntryID: " + entryInfo->getEntryID() +
      "; EntryType: " + StringTk::intToStr(entryInfo->getEntryType() ) +
      "; isBuddyMirrored: " + StringTk::intToStr(entryInfo->getIsBuddyMirrored() ) );

   StatData statData;
   FhgfsOpsErr statRes;

   NumNodeID parentNodeID;
   std::string parentEntryID;

   if(entryInfo->getParentEntryID().empty() || (entryInfo->getEntryID() == META_ROOTDIR_ID_STR) )
   { // special case: stat for root directory
      statRes = statRoot(statData);
   }
   else
   {
      statRes = MsgHelperStat::stat(entryInfo, true, getMsgHeaderUserID(), statData, &parentNodeID,
         &parentEntryID);
   }

   LOG_DBG(GENERAL, DEBUG, "", statRes);

   StatRespMsg respMsg(statRes, statData);

   if(isMsgHeaderFeatureFlagSet(STATMSG_FLAG_GET_PARENTINFO) && parentNodeID)
      respMsg.addParentInfo(parentNodeID, parentEntryID);

   ctx.sendResponse(respMsg);

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_STAT,
      getMsgHeaderUserID() );

   return true;
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

