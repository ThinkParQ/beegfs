#ifndef GETTARGETMAPPINGSMSG_H_
#define GETTARGETMAPPINGSMSG_H_

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


#endif /* GETTARGETMAPPINGSMSG_H_ */
