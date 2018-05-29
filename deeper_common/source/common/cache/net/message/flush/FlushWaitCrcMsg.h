#ifndef NET_MESSAGE_FLUSHWAITCRCMSG_H_
#define NET_MESSAGE_FLUSHWAITCRCMSG_H_


#include <common/cache/net/message/CacheMsg.h>
#include <common/net/message/NetMessageTypes.h>



class FlushWaitCrcMsg : public CacheMsg
{
   public:
      FlushWaitCrcMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CacheFlushCrcWait, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      FlushWaitCrcMsg() : CacheMsg(NETMSGTYPE_CacheFlushCrcWait) {};

      virtual ~FlushWaitCrcMsg() {};

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

#endif /* NET_MESSAGE_FLUSHWAITCRCMSG_H_ */
