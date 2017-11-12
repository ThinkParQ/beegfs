#ifndef NET_MESSAGE_PREFETCHRESPMSG_H_
#define NET_MESSAGE_PREFETCHRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class PrefetchRespMsg : public SimpleIntMsg
{
   public:
      PrefetchRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CachePrefetchResp, error) {};

      /*
       * deserialization only
       */
      PrefetchRespMsg() : SimpleIntMsg(NETMSGTYPE_CachePrefetchResp) {};

      virtual ~PrefetchRespMsg() {};
};

#endif /* NET_MESSAGE_PREFETCHRESPMSG_H_ */
