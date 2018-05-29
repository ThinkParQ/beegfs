#ifndef NET_MESSAGE_FLUSH_FLUSHWAITMSGEX_H_
#define NET_MESSAGE_FLUSH_FLUSHWAITMSGEX_H_


#include <common/cache/net/message/flush/FlushWaitMsg.h>



class FlushWaitMsgEx: public FlushWaitMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_FLUSHWAITMSGEX_H_ */
