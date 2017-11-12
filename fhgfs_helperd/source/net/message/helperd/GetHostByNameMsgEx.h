#ifndef GETHOSTBYNAMEMSGEX_H_
#define GETHOSTBYNAMEMSGEX_H_

#include <common/net/message/helperd/GetHostByNameMsg.h>

class GetHostByNameMsgEx : public GetHostByNameMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*GETHOSTBYNAMEMSGEX_H_*/
