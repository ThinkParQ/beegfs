#ifndef NET_MESSAGE_FLUSH_FLUSHSTOPMSGEX_H_
#define NET_MESSAGE_FLUSH_FLUSHSTOPMSGEX_H_


#include <common/cache/net/message/flush/FlushStopMsg.h>



class FlushStopMsgEx: public FlushStopMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_FLUSHSTOPMSGEX_H_ */
