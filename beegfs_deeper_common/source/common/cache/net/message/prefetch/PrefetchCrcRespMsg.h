#ifndef NET_MESSAGE_PREFETCHCRCRESPMSG_H_
#define NET_MESSAGE_PREFETCHCRCRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class PrefetchCrcRespMsg : public SimpleIntMsg
{
   public:
      PrefetchCrcRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CachePrefetchCrcResp, error) {};

      /*
       * deserialization only
       */
      PrefetchCrcRespMsg() : SimpleIntMsg(NETMSGTYPE_CachePrefetchCrcResp) {};

      virtual ~PrefetchCrcRespMsg() {};
};

#endif /* NET_MESSAGE_PREFETCHCRCRESPMSG_H_ */
