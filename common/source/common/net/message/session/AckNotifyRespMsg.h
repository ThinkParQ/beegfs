#ifndef COMMON_ACKNOTIFYRESPMSG
#define COMMON_ACKNOTIFYRESPMSG

#include <common/net/message/SimpleIntMsg.h>

class AckNotifiyRespMsg : public SimpleIntMsg
{
   public:
      AckNotifiyRespMsg(FhgfsOpsErr result):
         SimpleIntMsg(NETMSGTYPE_AckNotifyResp, result)
      {}

      AckNotifiyRespMsg(): SimpleIntMsg(NETMSGTYPE_AckNotifyResp) {}

      FhgfsOpsErr getResult() { return static_cast<FhgfsOpsErr>(getValue()); }
};

#endif
