#ifndef NET_MESSAGE_FLUSHCRCMSG_H_
#define NET_MESSAGE_FLUSHCRCMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include "../CacheMsg.h"



class FlushCrcMsg : public CacheMsg
{
   public:
      FlushCrcMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CacheFlushCrc, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      FlushCrcMsg() : CacheMsg(NETMSGTYPE_CacheFlushCrc) {};

      virtual ~FlushCrcMsg() {};

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

#endif /* NET_MESSAGE_FLUSHCRCMSG_H_ */
