#ifndef NET_MESSAGE_FLUSH_FLUSHRANGEMSGEX_H_
#define NET_MESSAGE_FLUSH_FLUSHRANGEMSGEX_H_


#include <common/cache/net/message/flush/FlushRangeMsg.h>



class FlushRangeMsgEx: public FlushRangeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_FLUSHRANGEMSGEX_H_ */
