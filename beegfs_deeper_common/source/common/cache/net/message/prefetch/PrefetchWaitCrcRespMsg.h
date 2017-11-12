#ifndef NET_MESSAGE_PREFETCHWAITCRCRESPMSG_H_
#define NET_MESSAGE_PREFETCHWAITCRCRESPMSG_H_


#include <common/cache/net/message/CacheWaitCrcRespMsg.h>
#include <common/net/message/NetMessageTypes.h>



class PrefetchWaitCrcRespMsg : public CacheWaitCrcRespMsg
{
   public:
      PrefetchWaitCrcRespMsg(int error, unsigned long checksum)
         : CacheWaitCrcRespMsg(NETMSGTYPE_CachePrefetchCrcWaitResp, error, checksum) {};

      /*
       * deserialization only
       */
      PrefetchWaitCrcRespMsg() : CacheWaitCrcRespMsg(NETMSGTYPE_CachePrefetchCrcWaitResp) {};

      virtual ~PrefetchWaitCrcRespMsg() {};
};

#endif /* NET_MESSAGE_PREFETCHWAITCRCRESPMSG_H_ */
