#ifndef NET_MESSAGE_PREFETCHSTOPMSG_H_
#define NET_MESSAGE_PREFETCHSTOPMSG_H_


#include <common/cache/net/message/CacheMsg.h>



class PrefetchStopMsg : public CacheMsg
{
   public:
      PrefetchStopMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CachePrefetchStop, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      PrefetchStopMsg() : CacheMsg(NETMSGTYPE_CachePrefetchStop) {};

      virtual ~PrefetchStopMsg() {};

      /**
       * Generates a key for the hash map.
       *
       * @param outKey The update key for the hash map.
       */
      void getKey(CacheWorkKey& outKey)
      {
         CacheMsg::getKey(outKey);
         outKey.type = CacheWorkType_PREFETCH;
      }
};

#endif /* NET_MESSAGE_PREFETCHSTOPMSG_H_ */
