#ifndef NET_MESSAGE_PREFETCHMSG_H_
#define NET_MESSAGE_PREFETCHMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include "../CacheMsg.h"



class PrefetchMsg : public CacheMsg
{
   public:
      PrefetchMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CachePrefetch, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      PrefetchMsg() : CacheMsg(NETMSGTYPE_CachePrefetch) {};

      virtual ~PrefetchMsg() {};

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

#endif /* NET_MESSAGE_PREFETCHMSG_H_ */
