#ifndef AUTHENTICATECHANNELMSG_H_
#define AUTHENTICATECHANNELMSG_H_

#include <common/net/message/SimpleInt64Msg.h>


class AuthenticateChannelMsg : public SimpleInt64Msg
{
   public:
      AuthenticateChannelMsg(uint64_t authHash) :
         SimpleInt64Msg(NETMSGTYPE_AuthenticateChannel, authHash)
      {
      }

      /**
       * For deserialization only
       */
      AuthenticateChannelMsg() : SimpleInt64Msg(NETMSGTYPE_AuthenticateChannel)
      {
      }


   private:


   public:
      // getters & setters
      uint64_t getAuthHash()
      {
         return getValue();
      }

};

#endif /* AUTHENTICATECHANNELMSG_H_ */
