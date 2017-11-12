#include <program/Program.h>
#include <common/net/message/storage/attribs/ListXAttrRespMsg.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include "ListXAttrMsgEx.h"


bool ListXAttrMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   const char* logContext = "ListXAttrMsg incoming";
   Config* config = app->getConfig();
   EntryInfo* entryInfo = this->getEntryInfo();
   size_t listSize = 0;

   LOG_DEBUG(logContext, Log_DEBUG, "Received a ListXAttrMsg from: " + ctx.peerName()
      + "; size: " + StringTk::intToStr(this->getSize()) + ";");

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
   ctx.sendResponse(ListXAttrRespMsg(xAttrVec, listSize, listXAttrRes) );

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_LISTXATTR,
         getMsgHeaderUserID() );

   return true;
}
