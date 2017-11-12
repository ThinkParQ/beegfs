#ifndef NET_MESSAGE_PREFETCHWAITMSG_H_
#define NET_MESSAGE_PREFETCHWAITMSG_H_


#include <common/cache/net/message/CacheMsg.h>
#include <common/net/message/NetMessageTypes.h>



class PrefetchWaitMsg : public CacheMsg
{
   public:
      PrefetchWaitMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CachePrefetchWait, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      PrefetchWaitMsg() : CacheMsg(NETMSGTYPE_CachePrefetchWait) {};

      virtual ~PrefetchWaitMsg() {};

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

#endif /* NET_MESSAGE_PREFETCHWAITMSG_H_ */
