#ifndef NET_MESSAGE_FLUSHRANGEMSG_H_
#define NET_MESSAGE_FLUSHRANGEMSG_H_


#include <common/cache/net/message/CacheCopyRangeMsg.h>
#include <common/net/message/NetMessageTypes.h>



class FlushRangeMsg : public CacheCopyRangeMsg
{
   public:
      FlushRangeMsg(std::string sourcePath, std::string destPath, int cacheFlags, off_t startPos,
         size_t numBytes) : CacheCopyRangeMsg(NETMSGTYPE_CacheFlushRange, sourcePath, destPath,
         cacheFlags, startPos, numBytes) {};

      /*
       * deserialization only
       */
      FlushRangeMsg() : CacheCopyRangeMsg(NETMSGTYPE_CacheFlushRange) {};

      virtual ~FlushRangeMsg() {};

      /**
       * Generates a key for the hash map.
       *
       * @param outKey The update key for the hash map.
       */
      void getKey(CacheWorkKey& outKey)
      {
         CacheCopyRangeMsg::getKey(outKey);
         outKey.type = CacheWorkType_FLUSH;
      }
};

#endif /* NET_MESSAGE_FLUSHRANGEMSG_H_ */
