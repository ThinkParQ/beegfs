#ifndef REFRESHSESSIONRESPMSG_H_
#define REFRESHSESSIONRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class RefreshSessionRespMsg : public SimpleIntMsg
{
   public:
      RefreshSessionRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RefreshSessionResp, result)
      {
      }

      RefreshSessionRespMsg() : SimpleIntMsg(NETMSGTYPE_RefreshSessionResp)
      {
      }
};

#endif /* REFRESHSESSIONRESPMSG_H_ */
