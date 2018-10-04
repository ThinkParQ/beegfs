#include <program/Program.h>
#include <common/net/message/storage/attribs/GetXAttrRespMsg.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include "GetXAttrMsgEx.h"


bool GetXAttrMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   const char* logContext = "GetXAttrMsg incoming";
   Config* config = app->getConfig();
   EntryInfo* entryInfo = this->getEntryInfo();
   ssize_t size = this->getSize();
   const std::string& name = this->getName();

   LOG_DEBUG(logContext, Log_DEBUG, "name: " + name + "; size: " + StringTk::intToStr(size) + ";");

   CharVector xAttrValue;
   FhgfsOpsErr getXAttrRes;

   if (!config->getStoreClientXAttrs() )
   {
      LogContext(logContext).log(Log_ERR,
         "Received a GetXAttrMsg, but client-side extended attributes are disabled in config.");

      getXAttrRes = FhgfsOpsErr_NOTSUPP;
      goto resp;
   }

   // Clamp buffer size to the maximum the NetMsg can handle, plus one byte in the (unlikely) case
   // the on-disk metadata is larger than that.
   if(size > MsgHelperXAttr::MAX_VALUE_SIZE)
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
   ctx.sendResponse(GetXAttrRespMsg(xAttrValue, size, getXAttrRes) );

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_GETXATTR,
      getMsgHeaderUserID() );

   return true;
}
