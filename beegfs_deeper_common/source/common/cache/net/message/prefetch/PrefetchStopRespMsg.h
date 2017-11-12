#ifndef NET_MESSAGE_PREFETCHSTOPRESPMSG_H_
#define NET_MESSAGE_PREFETCHSTOPRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class PrefetchStopRespMsg : public SimpleIntMsg
{
   public:
      PrefetchStopRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CachePrefetchStopResp, error) {};

      /*
       * deserialization only
       */
      PrefetchStopRespMsg() : SimpleIntMsg(NETMSGTYPE_CachePrefetchStopResp) {};

      virtual ~PrefetchStopRespMsg() {};
};

#endif /* NET_MESSAGE_PREFETCHSTOPRESPMSG_H_ */
