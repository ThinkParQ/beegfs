#ifndef NET_MESSAGE_PREFETCHRANGEMSG_H_
#define NET_MESSAGE_PREFETCHRANGEMSG_H_


#include <common/cache/net/message/CacheCopyRangeMsg.h>
#include <common/net/message/NetMessageTypes.h>



class PrefetchRangeMsg : public CacheCopyRangeMsg
{
   public:
      PrefetchRangeMsg(std::string sourcePath, std::string destPath, int cacheFlags, off_t startPos,
         size_t numBytes) : CacheCopyRangeMsg(NETMSGTYPE_CachePrefetchRange, sourcePath, destPath,
         cacheFlags, startPos, numBytes) {};

      /*
       * deserialization only
       */
      PrefetchRangeMsg() : CacheCopyRangeMsg(NETMSGTYPE_CachePrefetchRange) {};

      virtual ~PrefetchRangeMsg() {};

      /**
       * Generates a key for the hash map.
       *
       * @param outKey The update key for the hash map.
       */
      void getKey(CacheWorkKey& outKey)
      {
         CacheCopyRangeMsg::getKey(outKey);
         outKey.type = CacheWorkType_PREFETCH;
      }
};

#endif /* NET_MESSAGE_PREFETCHRANGEMSG_H_ */
