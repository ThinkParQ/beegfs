#ifndef NET_MESSAGE_FLUSHWAITMSG_H_
#define NET_MESSAGE_FLUSHWAITMSG_H_


#include <common/cache/net/message/CacheMsg.h>
#include <common/net/message/NetMessageTypes.h>



class FlushWaitMsg : public CacheMsg
{
   public:
      FlushWaitMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CacheFlushWait, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      FlushWaitMsg() : CacheMsg(NETMSGTYPE_CacheFlushWait) {};

      virtual ~FlushWaitMsg() {};

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

#endif /* NET_MESSAGE_FLUSHWAITMSG_H_ */
