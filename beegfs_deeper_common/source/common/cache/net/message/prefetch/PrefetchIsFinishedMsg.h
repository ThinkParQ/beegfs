#ifndef NET_MESSAGE_PREFETCHISFINISHEDMSG_H_
#define NET_MESSAGE_PREFETCHISFINISHEDMSG_H_


#include <common/cache/net/message/CacheMsg.h>



class PrefetchIsFinishedMsg : public CacheMsg
{
   public:
      PrefetchIsFinishedMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CachePrefetchIsFinished, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      PrefetchIsFinishedMsg() : CacheMsg(NETMSGTYPE_CachePrefetchIsFinished) {};

      virtual ~PrefetchIsFinishedMsg() {};

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

#endif /* NET_MESSAGE_PREFETCHISFINISHEDMSG_H_ */
