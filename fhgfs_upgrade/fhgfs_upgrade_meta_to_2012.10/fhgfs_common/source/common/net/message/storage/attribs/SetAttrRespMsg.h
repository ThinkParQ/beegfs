#ifndef SETATTRRESPMSG_H_
#define SETATTRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class SetAttrRespMsg : public SimpleIntMsg
{
   public:
      SetAttrRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetAttrResp, result)
      {
      }

      SetAttrRespMsg() : SimpleIntMsg(NETMSGTYPE_SetAttrResp)
      {
      }
};

#endif /*SETATTRRESPMSG_H_*/
