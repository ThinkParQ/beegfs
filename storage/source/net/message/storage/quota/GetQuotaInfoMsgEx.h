#pragma once


#include <common/net/message/storage/quota/GetQuotaInfoMsg.h>
#include <common/Common.h>



class GetQuotaInfoMsgEx : public GetQuotaInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

