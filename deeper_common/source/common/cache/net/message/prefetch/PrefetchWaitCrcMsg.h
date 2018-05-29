#ifndef NET_MESSAGE_PREFETCHWAITCRCMSG_H_
#define NET_MESSAGE_PREFETCHWAITCRCMSG_H_


#include <common/cache/net/message/CacheMsg.h>
#include <common/net/message/NetMessageTypes.h>



class PrefetchWaitCrcMsg : public CacheMsg
{
   public:
      PrefetchWaitCrcMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CachePrefetchCrcWait, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      PrefetchWaitCrcMsg() : CacheMsg(NETMSGTYPE_CachePrefetchCrcWait) {};

      virtual ~PrefetchWaitCrcMsg() {};

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

#endif /* NET_MESSAGE_PREFETCHWAITCRCMSG_H_ */
