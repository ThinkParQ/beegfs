#include "RetrieveInodesMsgEx.h"

#include <program/Program.h>

bool RetrieveInodesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Incoming RetrieveDirEntriesMsg");

   MetaStore *metaStore = Program::getApp()->getMetaStore();

   unsigned hashDirNum = getHashDirNum();
   unsigned maxOutInodes = getMaxOutInodes();
   int64_t lastOffset = getLastOffset();
   int64_t newOffset;

   FsckFileInodeList fileInodesOutgoing;
   FsckDirInodeList dirInodesOutgoing;

   metaStore->getAllInodesIncremental(hashDirNum, lastOffset, maxOutInodes, &dirInodesOutgoing,
      &fileInodesOutgoing, &newOffset, getIsBuddyMirrored());

   ctx.sendResponse(RetrieveInodesRespMsg(&fileInodesOutgoing, &dirInodesOutgoing, newOffset) );

   return true;
}
