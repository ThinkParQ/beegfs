#pragma once


#include <common/net/message/storage/quota/SetExceededQuotaMsg.h>
#include <common/Common.h>


class SetExceededQuotaMsgEx : public SetExceededQuotaMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

