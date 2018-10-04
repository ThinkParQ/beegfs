#ifndef NETMESSAGEFACTORY_H_
#define NETMESSAGEFACTORY_H_

#include <common/Common.h>
#include <common/net/message/AbstractNetMessageFactory.h>

class NetMessageFactory : public AbstractNetMessageFactory
{
   public:
      NetMessageFactory() {}

   protected:
      virtual std::unique_ptr<NetMessage> createFromMsgType(unsigned short msgType) const override;
} ;

#endif /*NETMESSAGEFACTORY_H_*/
