#ifndef DEEPER_LIB_NETMESSAGEFACTORY_H_
#define DEEPER_LIB_NETMESSAGEFACTORY_H_

#include <common/Common.h>
#include <common/net/message/AbstractNetMessageFactory.h>

class NetMessageFactory : public AbstractNetMessageFactory
{
   public:
      NetMessageFactory() {}

   protected:
      virtual NetMessage* createFromMsgType(unsigned short msgType);
} ;

#endif /*DEEPER_LIB_NETMESSAGEFACTORY_H_*/
