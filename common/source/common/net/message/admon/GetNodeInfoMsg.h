#ifndef GETNODEINFOMSG_H_
#define GETNODEINFOMSG_H_

#include <common/Common.h>
#include "../SimpleMsg.h"

class GetNodeInfoMsg : public SimpleMsg
{
   public:
	  GetNodeInfoMsg() : SimpleMsg(NETMSGTYPE_GetNodeInfo)
      {
      }
};


#endif /*GETNODEINFOMSG_H_*/
