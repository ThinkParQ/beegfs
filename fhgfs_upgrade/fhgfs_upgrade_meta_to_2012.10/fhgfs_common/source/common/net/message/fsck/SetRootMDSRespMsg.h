#ifndef SETROOTMDSRESPMSG_H
#define SETROOTMDSRESPMSG_H

#include <common/net/message/SimpleIntMsg.h>

class SetRootMDSRespMsg : public SimpleIntMsg
{
   public:
      SetRootMDSRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetRootMDSResp, result)
      {
      }


   protected:
      SetRootMDSRespMsg() : SimpleIntMsg(NETMSGTYPE_SetRootMDSResp)
      {
      }
};


#endif /*SETROOTMDSRESPMSG_H*/
