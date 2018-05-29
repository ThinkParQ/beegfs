#ifndef NET_MESSAGE_PREFETCH_PREFETCHCRCMSGEX_H_
#define NET_MESSAGE_PREFETCH_PREFETCHCRCMSGEX_H_


#include <common/cache/net/message/prefetch/PrefetchCrcMsg.h>



class PrefetchCrcMsgEx: public PrefetchCrcMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_PREFETCH_PREFETCHCRCMSGEX_H_ */
