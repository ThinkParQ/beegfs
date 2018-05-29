#ifndef NET_MESSAGE_PREFETCHWAITRESPMSG_H_
#define NET_MESSAGE_PREFETCHWAITRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class PrefetchWaitRespMsg : public SimpleIntMsg
{
   public:
      PrefetchWaitRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CachePrefetchWaitResp, error) {};

      /*
       * deserialization only
       */
      PrefetchWaitRespMsg() : SimpleIntMsg(NETMSGTYPE_CachePrefetchWaitResp) {};

      virtual ~PrefetchWaitRespMsg() {};
};

#endif /* NET_MESSAGE_PREFETCHWAITRESPMSG_H_ */
