#ifndef NET_MESSAGE_FLUSH_PREFETCHRANGEMSGEX_H_
#define NET_MESSAGE_FLUSH_PREFETCHRANGEMSGEX_H_


#include <common/cache/net/message/prefetch/PrefetchRangeMsg.h>



class PrefetchRangeMsgEx: public PrefetchRangeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_PREFETCHRANGEMSGEX_H_ */
