#ifndef SETROOTNODEIDRESPMSG_H
#define SETROOTNODEIDRESPMSG_H

#include <common/net/message/SimpleIntMsg.h>

class SetRootNodeIDRespMsg : public SimpleIntMsg
{
   public:
      SetRootNodeIDRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_SetRootNodeIDResp, result)
      {
      }

      /**
       * For deserialization only
       */
      SetRootNodeIDRespMsg() : SimpleIntMsg(NETMSGTYPE_SetRootNodeIDResp)
      {
      }
};


#endif /*SETROOTNODEIDRESPMSG_H*/
