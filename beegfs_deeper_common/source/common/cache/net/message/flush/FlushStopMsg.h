#ifndef NET_MESSAGE_FLUSHSTOPMSG_H_
#define NET_MESSAGE_FLUSHSTOPMSG_H_


#include <common/cache/net/message/CacheMsg.h>



class FlushStopMsg : public CacheMsg
{
   public:
      FlushStopMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CacheFlushStop, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      FlushStopMsg() : CacheMsg(NETMSGTYPE_CacheFlushStop) {};

      virtual ~FlushStopMsg() {};

      /**
       * Generates a key for the hash map.
       *
       * @param outKey The update key for the hash map.
       */
      void getKey(CacheWorkKey& outKey)
      {
         CacheMsg::getKey(outKey);
         outKey.type = CacheWorkType_FLUSH;
      }
};

#endif /* NET_MESSAGE_FLUSHSTOPMSG_H_ */
