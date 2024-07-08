#include <program/Program.h>
#include <common/net/message/storage/attribs/ListXAttrRespMsg.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include "ListXAttrMsgEx.h"

bool ListXAttrMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "ListXAttrMsg incoming";
   #endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_DEBUG, "size: " + StringTk::intToStr(this->getSize()) + ";");
   return BaseType::processIncoming(ctx);
}

FileIDLock ListXAttrMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), false};
}

std::unique_ptr<MirroredMessageResponseState> ListXAttrMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   const char* logContext = "List XAttr Msg";
   ListXAttrMsgResponseState resp;

   App* app = Program::getApp();
   Config* config = app->getConfig();
   EntryInfo* entryInfo = this->getEntryInfo();
   size_t listSize = 0;

   StringVector xAttrVec;
   FhgfsOpsErr listXAttrRes;

   if (!config->getStoreClientXAttrs() )
   {
      LogContext(logContext).log(Log_ERR,
         "Received a ListXAttrMsg, but client-side extended attributes are disabled in config.");

      listXAttrRes = FhgfsOpsErr_NOTSUPP;
      goto resp;
   }

   std::tie(listXAttrRes, xAttrVec) = MsgHelperXAttr::listxattr(entryInfo);

   for (StringVectorConstIter it = xAttrVec.begin(); it != xAttrVec.end(); ++it)
   {
      // Plus one byte to account for the '\0's separating the list elements in the buffer.
      listSize += it->size() + 1;
   }

   // note: MsgHelperXAttr::MAX_SIZE is a ssize_t, which is always positive, so it will always fit
   // into a size_t
   if ((listSize >= (size_t)MsgHelperXAttr::MAX_VALUE_SIZE + 1)
      && (listXAttrRes == FhgfsOpsErr_SUCCESS))
   {
      // The xattr list on disk is at least one byte too large. In this case, we have to return
      // an internal error because it won't fit the net message.
      xAttrVec.clear();
      listXAttrRes = FhgfsOpsErr_TOOBIG;
   }

resp:
   resp.setListXAttrValue(xAttrVec);
   resp.setSize(listSize);
   resp.setListXAttrResult(listXAttrRes);

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_LISTXATTR,
         getMsgHeaderUserID() );

   return boost::make_unique<ResponseState>(std::move(resp));
}
