#ifndef HEARTBEATREQUESTMSG_H_
#define HEARTBEATREQUESTMSG_H_

#include <common/Common.h>
#include "../SimpleMsg.h"

class HeartbeatRequestMsg : public SimpleMsg
{
   public:
      HeartbeatRequestMsg() : SimpleMsg(NETMSGTYPE_HeartbeatRequest)
      {
      }
      
      virtual TestingEqualsRes testingEquals(NetMessage *msg);
};


#endif /*HEARTBEATREQUESTMSG_H_*/
