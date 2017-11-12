#ifndef NET_MESSAGE_PREFETCHISFINISHEDRESPMSG_H_
#define NET_MESSAGE_PREFETCHISFINISHEDRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class PrefetchIsFinishedRespMsg : public SimpleIntMsg
{
   public:
      PrefetchIsFinishedRespMsg(int error)
         : SimpleIntMsg(NETMSGTYPE_CachePrefetchIsFinishedResp, error) {};

      /*
       * deserialization only
       */
      PrefetchIsFinishedRespMsg() : SimpleIntMsg(NETMSGTYPE_CachePrefetchIsFinishedResp) {};

      virtual ~PrefetchIsFinishedRespMsg() {};
};

#endif /* NET_MESSAGE_PREFETCHISFINISHEDRESPMSG_H_ */
