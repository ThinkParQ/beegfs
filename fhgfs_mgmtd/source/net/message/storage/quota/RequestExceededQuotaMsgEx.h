#ifndef REQUESTEXCEEDEDQUOTAMSGEX_H_
#define REQUESTEXCEEDEDQUOTAMSGEX_H_


#include <common/net/message/storage/quota/RequestExceededQuotaMsg.h>
#include <common/Common.h>


class RequestExceededQuotaMsgEx : public RequestExceededQuotaMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* REQUESTEXCEEDEDQUOTAMSGEX_H_ */
