#ifndef NET_MESSAGE_FLUSH_FLUSHMSGEX_H_
#define NET_MESSAGE_FLUSH_FLUSHMSGEX_H_


#include <common/cache/net/message/flush/FlushMsg.h>



class FlushMsgEx: public FlushMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_FLUSHMSGEX_H_ */
