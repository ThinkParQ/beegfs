#ifndef NETMESSAGEFACTORY_H_
#define NETMESSAGEFACTORY_H_

#include <common/Common.h>
#include <common/net/message/AbstractNetMessageFactory.h>

class NetMessageFactory : public AbstractNetMessageFactory
{
   protected:
      virtual NetMessage* createFromMsgType(unsigned short msgType) override;
} ;

#endif /*NETMESSAGEFACTORY_H_*/
