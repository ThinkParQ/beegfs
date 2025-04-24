#pragma once

#include <common/Common.h>
#include <common/net/message/SimpleMsg.h>


class GetTargetMappingsMsg : public SimpleMsg
{
   public:
      GetTargetMappingsMsg() :
         SimpleMsg(NETMSGTYPE_GetTargetMappings)
      {
      }

};


