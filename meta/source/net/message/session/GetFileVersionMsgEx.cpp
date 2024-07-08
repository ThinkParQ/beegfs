#include "GetFileVersionMsgEx.h"
#include <program/Program.h>

bool GetFileVersionMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(SESSIONS, DEBUG, "", getEntryInfo().getEntryID(), getEntryInfo().getIsBuddyMirrored());
   return BaseType::processIncoming(ctx);
}

FileIDLock GetFileVersionMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo().getEntryID(), false};
}

std::unique_ptr<MirroredMessageResponseState> GetFileVersionMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   GetFileVersionMsgResponseState resp;

   auto& metaStore = *Program::getApp()->getMetaStore();
   auto inode = metaStore.referenceFile(&getEntryInfo());
   if (!inode)
   {
      // The GetFileVersionMsgResponseState constructor sets default values for 'result' and
      // 'version', indicating an error condition, eliminating the need to specify them
      // separately. Hence, returning a ResponseState with the moved 'resp'.
      return boost::make_unique<ResponseState>(std::move(resp));
   }

   resp.setGetFileVersionResult(FhgfsOpsErr_SUCCESS);
   resp.setFileVersion(inode->getFileVersion());
   metaStore.releaseFile(getEntryInfo().getParentEntryID(), inode);

   return boost::make_unique<ResponseState>(std::move(resp));
}
