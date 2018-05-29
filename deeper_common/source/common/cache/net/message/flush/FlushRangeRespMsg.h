#ifndef NET_MESSAGE_FLUSHRANGERESPMSG_H_
#define NET_MESSAGE_FLUSHRANGERESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class FlushRangeRespMsg : public SimpleIntMsg
{
   public:
      FlushRangeRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CacheFlushRangeResp, error) {};

      /*
       * deserialization only
       */
      FlushRangeRespMsg() : SimpleIntMsg(NETMSGTYPE_CacheFlushRangeResp) {};

      virtual ~FlushRangeRespMsg() {};
};

#endif /* NET_MESSAGE_FLUSHRANGERESPMSG_H_ */
