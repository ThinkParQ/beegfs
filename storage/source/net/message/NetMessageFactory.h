#pragma once

#include <common/Common.h>
#include <common/net/message/AbstractNetMessageFactory.h>

class NetMessageFactory : public AbstractNetMessageFactory
{
   public:
      NetMessageFactory() {}

   protected:
      virtual std::unique_ptr<NetMessage> createFromMsgType(unsigned short msgType) const override;
} ;

