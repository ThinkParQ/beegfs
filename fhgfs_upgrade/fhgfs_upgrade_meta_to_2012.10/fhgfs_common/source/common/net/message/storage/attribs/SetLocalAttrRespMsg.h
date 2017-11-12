#ifndef SETLOCALATTRRESPMSG_H_
#define SETLOCALATTRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class SetLocalAttrRespMsg : public SimpleIntMsg
{
   public:
      SetLocalAttrRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetLocalAttrResp, result)
      {
      }

      SetLocalAttrRespMsg() : SimpleIntMsg(NETMSGTYPE_SetLocalAttrResp)
      {
      }
};


#endif /*SETLOCALATTRRESPMSG_H_*/
