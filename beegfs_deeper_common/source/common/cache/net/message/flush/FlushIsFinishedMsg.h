#ifndef NET_MESSAGE_FLUSHISFINISHEDMSG_H_
#define NET_MESSAGE_FLUSHISFINISHEDMSG_H_


#include <common/cache/net/message/CacheMsg.h>



class FlushIsFinishedMsg : public CacheMsg
{
   public:
      FlushIsFinishedMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CacheFlushIsFinished, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      FlushIsFinishedMsg() : CacheMsg(NETMSGTYPE_CacheFlushIsFinished) {};

      virtual ~FlushIsFinishedMsg() {};

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

#endif /* NET_MESSAGE_FLUSHISFINISHEDMSG_H_ */
