#ifndef NET_MESSAGE_FLUSHMSG_H_
#define NET_MESSAGE_FLUSHMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include "../CacheMsg.h"



class FlushMsg : public CacheMsg
{
   public:
      FlushMsg(std::string sourcePath, std::string destPath, int cacheFlags) :
         CacheMsg(NETMSGTYPE_CacheFlush, sourcePath, destPath, cacheFlags) {};

      /*
       * deserialization only
       */
      FlushMsg() : CacheMsg(NETMSGTYPE_CacheFlush) {};

      virtual ~FlushMsg() {};

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

#endif /* NET_MESSAGE_FLUSHMSG_H_ */
