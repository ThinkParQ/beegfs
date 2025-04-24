#pragma once

#include <app/App.h>
#include <common/app/log/LogContext.h>
#include <common/net/message/mon/RequestMetaDataMsg.h>
#include <common/net/message/mon/RequestMetaDataRespMsg.h>

class RequestMetaDataMsgEx : public RequestMetaDataMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

