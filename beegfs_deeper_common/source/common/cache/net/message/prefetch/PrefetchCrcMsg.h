#ifndef NET_MESSAGE_PREFETCHCRCMSG_H_
#define NET_MESSAGE_PREFETCHCRCMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include "../CacheMsg.h"



class PrefetchCrcMsg : public CacheMsg
{
   public:
      PrefetchCrcMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CachePrefetchCrc, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      PrefetchCrcMsg() : CacheMsg(NETMSGTYPE_CachePrefetchCrc) {};

      virtual ~PrefetchCrcMsg() {};

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

#endif /* NET_MESSAGE_PREFETCHCRCMSG_H_ */
