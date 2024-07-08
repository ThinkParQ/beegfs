#include <program/Program.h>
#include <common/net/message/storage/attribs/GetXAttrRespMsg.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include "GetXAttrMsgEx.h"

bool GetXAttrMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetXAttrMsg incoming";
   LogContext(logContext).log(Log_DEBUG , "name: " + this->getName() + "; size: " +
      StringTk::intToStr(this->getSize()) + ";");
   return BaseType::processIncoming(ctx);
}

FileIDLock GetXAttrMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), false};
}

std::unique_ptr<MirroredMessageResponseState> GetXAttrMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   const char* logContext = "Get XAttr Msg";
   GetXAttrMsgResponseState resp;

   EntryInfo* entryInfo = this->getEntryInfo();
   CharVector xAttrValue;
   FhgfsOpsErr getXAttrRes;
   ssize_t size = this->getSize();
   const std::string& name = this->getName();

   App* app = Program::getApp();
   Config* config = app->getConfig();
   if (!config->getStoreClientXAttrs())
   {
      LogContext(logContext).log(Log_ERR,
         "Received a GetXAttrMsg, but client-side extended attributes are disabled in config.");

      getXAttrRes = FhgfsOpsErr_NOTSUPP;
      goto resp;
   }

   // Clamp buffer size to the maximum the NetMsg can handle, plus one byte in the (unlikely) case
   // the on-disk metadata is larger than that.
   if (size > MsgHelperXAttr::MAX_VALUE_SIZE)
      size = MsgHelperXAttr::MAX_VALUE_SIZE + 1;

   std::tie(getXAttrRes, xAttrValue, size) = MsgHelperXAttr::getxattr(entryInfo, name, size);

   if (size >= MsgHelperXAttr::MAX_VALUE_SIZE + 1 && getXAttrRes == FhgfsOpsErr_SUCCESS)
   {
      // The xattr on disk is at least one byte too large. In this case, we have to return
      // an internal error because it won't fit the net message.
      xAttrValue.clear();
      getXAttrRes = FhgfsOpsErr_INTERNAL;
   }

resp:
   resp.setGetXAttrValue(xAttrValue);
   resp.setSize(size);
   resp.setGetXAttrResult(getXAttrRes);

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_GETXATTR,
      getMsgHeaderUserID() );

   return boost::make_unique<ResponseState>(std::move(resp));
}
