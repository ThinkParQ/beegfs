#ifndef NET_MESSAGE_PREFETCHRANGERESPMSG_H_
#define NET_MESSAGE_PREFETCHRANGERESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class PrefetchRangeRespMsg : public SimpleIntMsg
{
   public:
      PrefetchRangeRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CachePrefetchRangeResp, error) {};

      /*
       * deserialization only
       */
      PrefetchRangeRespMsg() : SimpleIntMsg(NETMSGTYPE_CachePrefetchRangeResp) {};

      virtual ~PrefetchRangeRespMsg() {};
};

#endif /* NET_MESSAGE_PREFETCHRANGERESPMSG_H_ */
