#include <program/Program.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include "ListDirFromOffsetMsgEx.h"

#include <boost/lexical_cast.hpp>


bool ListDirFromOffsetMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "ListDirFromOffsetMsgEx incoming";
   #endif // BEEGFS_DEBUG

   EntryInfo* entryInfo = this->getEntryInfo();

   LOG_DEBUG(logContext, Log_SPAM,
      std::string("serverOffset: ")  + StringTk::int64ToStr(getServerOffset() )             + "; " +
      std::string("maxOutNames: ")   + StringTk::int64ToStr(getMaxOutNames() )              + "; " +
      std::string("filterDots: ")    + StringTk::uintToStr(getFilterDots() )                + "; " +
      std::string("parentEntryID: ") + entryInfo->getParentEntryID()                        + "; " +
      std::string("buddyMirrored: ") + (entryInfo->getIsBuddyMirrored() ? "true" : "false") + "; " +
      std::string("entryID: ")       + entryInfo->getEntryID() );
   
   StringList names;
   UInt8List entryTypes;
   StringList entryIDs;
   Int64List serverOffsets;
   int64_t newServerOffset = getServerOffset(); // init to something useful

   FhgfsOpsErr listRes = listDirIncremental(entryInfo, &names, &entryTypes, &entryIDs,
      &serverOffsets, &newServerOffset);
   
   LOG_DEBUG(logContext, Log_SPAM,
      std::string("newServerOffset: ") + StringTk::int64ToStr(newServerOffset) + "; " +
      std::string("names.size: ") + StringTk::int64ToStr(names.size() ) + "; " +
      std::string("listRes: ") + boost::lexical_cast<std::string>(listRes));

   ctx.sendResponse(
         ListDirFromOffsetRespMsg(
            listRes, &names, &entryTypes, &entryIDs, &serverOffsets, newServerOffset) );

   Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      MetaOpCounter_READDIR, getMsgHeaderUserID() );

   return true;
}

FhgfsOpsErr ListDirFromOffsetMsgEx::listDirIncremental(EntryInfo* entryInfo, StringList* outNames,
   UInt8List* outEntryTypes, StringList* outEntryIDs, Int64List* outServerOffsets,
   int64_t* outNewOffset)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   // reference dir
   DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored(),
      true);
   if(!dir)
      return FhgfsOpsErr_PATHNOTEXISTS;
   
   // query contents
   ListIncExOutArgs outArgs(outNames, outEntryTypes, outEntryIDs, outServerOffsets, outNewOffset);

   FhgfsOpsErr listRes = dir->listIncrementalEx(
      getServerOffset(), getMaxOutNames(), getFilterDots(), outArgs);

   // clean-up
   metaStore->releaseDir(entryInfo->getEntryID() );
   
   return listRes;
}

