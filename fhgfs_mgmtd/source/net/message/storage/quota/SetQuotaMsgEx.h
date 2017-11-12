#ifndef SETQUOTAMSGEX_H_
#define SETQUOTAMSGEX_H_


#include <common/net/message/storage/quota/SetQuotaMsg.h>


class SetQuotaMsgEx : public SetQuotaMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      bool processQuotaLimits();
};

#endif /* SETQUOTAMSGEX_H_ */
