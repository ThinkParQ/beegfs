#ifndef GETQUOTAINFOMSGEX_H_
#define GETQUOTAINFOMSGEX_H_


#include <common/net/message/storage/quota/GetQuotaInfoMsg.h>
#include <common/Common.h>



class GetQuotaInfoMsgEx : public GetQuotaInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* GETQUOTAINFOMSGEX_H_ */
