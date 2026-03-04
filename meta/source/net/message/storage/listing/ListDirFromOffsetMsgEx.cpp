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
   FhgfsOpsErr listRes = FhgfsOpsErr_INTERNAL;

   LOG_DEBUG(logContext, Log_SPAM,
      std::string("serverOffset: ")  + StringTk::int64ToStr(getServerOffset() )             + "; " +
      std::string("dirListLimit: ")  + StringTk::uintToStr(getDirListLimit() )              + "; " +
      std::string("filterDots: ")    + StringTk::uintToStr(getFilterDots() )                + "; " +
      std::string("parentEntryID: ") + entryInfo->getParentEntryID()                        + "; " +
      std::string("buddyMirrored: ") + (entryInfo->getIsBuddyMirrored() ? "true" : "false") + "; " +
      std::string("entryID: ")       + entryInfo->getEntryID() );

   StringList names;
   UInt8List entryTypes;
   StringList entryIDs;
   Int64List serverOffsets;
   int64_t newServerOffset = getServerOffset(); // init to something useful

   unsigned respBufSize = 0;
   if (isMsgHeaderCompatFeatureFlagSet(LISTDIROFFSETMSG_COMPATFLAG_CLIENT_SUPPORTS_BUFSIZE))
   {
      const unsigned clientBufSize = getDirListLimit();
      const unsigned metaBufSize = Program::getApp()->getConfig()->getTuneWorkerBufSize();

      // Max possible size for the serialized response is the minimum of the
      // client's buffer size and the meta server's worker buffer size
      respBufSize = std::min(clientBufSize, metaBufSize);

      // Fixed response fields: newServerOffset(8 bytes) + result(4 bytes)
      // List serialization overhead (32 bytes for 4 lists) is handled by listIncrementalEx()
      const unsigned FIXED_RESPONSE_FIELDS_SIZE = sizeof(newServerOffset) + sizeof(listRes);

      // Adjust respBufSize to account for message header and fixed fields
      respBufSize = (respBufSize > NETMSG_HEADER_LENGTH + FIXED_RESPONSE_FIELDS_SIZE)
         ? respBufSize - NETMSG_HEADER_LENGTH - FIXED_RESPONSE_FIELDS_SIZE
         : 0;
   }
   // else: respBufSize = 0, will use dirListLimit as entry count limit

   listRes = listDirIncremental(entryInfo, &names, &entryTypes, &entryIDs,
      &serverOffsets, &newServerOffset, respBufSize);

   LOG_DEBUG(logContext, Log_SPAM,
      std::string("newServerOffset: ") + StringTk::int64ToStr(newServerOffset) + "; " +
      std::string("names.size: ") + StringTk::int64ToStr(names.size() ) + "; " +
      std::string("listRes: ") + boost::lexical_cast<std::string>(listRes));

   auto respMsg = ListDirFromOffsetRespMsg(
      listRes, &names, &entryTypes, &entryIDs, &serverOffsets, newServerOffset);

   // Always advertise that server supports buffer size mode (for auto-detection)
   // Client will detect this and enable buffer mode on subsequent requests
   respMsg.addMsgHeaderCompatFeatureFlag(LISTDIROFFSETRESPMSG_COMPATFLAG_SERVER_SUPPORTS_BUFSIZE);

   ctx.sendResponse(respMsg);

   Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      MetaOpCounter_READDIR, getMsgHeaderUserID() );

   return true;
}

FhgfsOpsErr ListDirFromOffsetMsgEx::listDirIncremental(EntryInfo* entryInfo, StringList* outNames,
   UInt8List* outEntryTypes, StringList* outEntryIDs, Int64List* outServerOffsets,
   int64_t* outNewOffset, unsigned respBufSize)
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
      getServerOffset(), getDirListLimit(), getFilterDots(), outArgs, respBufSize);

   // clean-up
   metaStore->releaseDir(entryInfo->getEntryID() );

   return listRes;
}

