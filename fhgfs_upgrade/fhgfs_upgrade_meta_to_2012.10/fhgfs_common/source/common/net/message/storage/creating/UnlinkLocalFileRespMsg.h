#ifndef UNLINKLOCALFILERESPMSG_H_
#define UNLINKLOCALFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class UnlinkLocalFileRespMsg : public SimpleIntMsg
{
   public:
      UnlinkLocalFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_UnlinkLocalFileResp, result)
      {
      }

      UnlinkLocalFileRespMsg() : SimpleIntMsg(NETMSGTYPE_UnlinkLocalFileResp)
      {
      }
};

#endif /*UNLINKLOCALFILERESPMSG_H_*/
