#ifndef MKLOCALFILERESPMSG_H_
#define MKLOCALFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class MkLocalFileRespMsg : public SimpleIntMsg
{
   public:
      MkLocalFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_MkLocalFileResp, result)
      {
      }

      MkLocalFileRespMsg() : SimpleIntMsg(NETMSGTYPE_MkLocalFileResp)
      {
      }
};

#endif /*MKLOCALFILERESPMSG_H_*/
