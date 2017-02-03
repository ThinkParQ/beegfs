#include "GetFileVersionMsgEx.h"

#include <program/Program.h>

bool GetFileVersionMsgEx::processIncoming(ResponseContext& ctx)
{
   auto& metaStore = *Program::getApp()->getMetaStore();
   LOG_DBG(DEBUG, "Received a GetFileVersionMsg.", ctx.peerName());
   LOG_DBG(DEBUG, "", getEntryInfo().getEntryID(), getEntryInfo().getIsBuddyMirrored());

   auto inode = metaStore.referenceFile(&getEntryInfo());
   if (!inode)
   {
      ctx.sendResponse(GetFileVersionRespMsg(FhgfsOpsErr_INTERNAL, 0));
      return true;
   }

   ctx.sendResponse(GetFileVersionRespMsg(FhgfsOpsErr_SUCCESS, inode->getFileVersion()));
   metaStore.releaseFile(getEntryInfo().getParentEntryID(), inode);
   return true;
}
