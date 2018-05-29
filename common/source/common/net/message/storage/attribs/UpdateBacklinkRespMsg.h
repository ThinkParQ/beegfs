#ifndef UPDATEBACKLINKRESPMSG_H_
#define UPDATEBACKLINKRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class UpdateBacklinkRespMsg : public SimpleIntMsg
{
   public:
      UpdateBacklinkRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_UpdateBacklinkResp, result)
      {
      }

      UpdateBacklinkRespMsg() : SimpleIntMsg(NETMSGTYPE_UpdateBacklinkResp)
      {
      }
};

#endif /* UPDATEBACKLINKRESPMSG_H_ */
