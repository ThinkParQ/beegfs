#include "GetFileVersionMsgEx.h"

#include <program/Program.h>

bool GetFileVersionMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(DEBUG, "Received a GetFileVersionMsg.", ctx.peerName());
   LOG_DBG(DEBUG, "", getEntryInfo().getEntryID(), getEntryInfo().getIsBuddyMirrored());

   auto inode = Program::getApp()->getMetaStore()->referenceFile(&getEntryInfo());
   if (!inode)
   {
      ctx.sendResponse(GetFileVersionRespMsg(FhgfsOpsErr_INTERNAL, 0));
      return true;
   }

   ctx.sendResponse(GetFileVersionRespMsg(FhgfsOpsErr_SUCCESS, inode->getFileVersion()));
   return true;
}
