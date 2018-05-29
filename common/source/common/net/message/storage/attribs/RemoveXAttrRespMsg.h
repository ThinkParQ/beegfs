#ifndef REMOVEXATTRRESPMSG_H_
#define REMOVEXATTRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class RemoveXAttrRespMsg : public SimpleIntMsg
{
   public:
      RemoveXAttrRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RemoveXAttrResp, result) {}

      RemoveXAttrRespMsg() : SimpleIntMsg(NETMSGTYPE_RemoveXAttrResp) {}
};

#endif /*REMOVEXATTRRESPMSG_H_*/
