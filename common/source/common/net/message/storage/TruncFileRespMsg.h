#ifndef TRUNCFILERESPMSG_H_
#define TRUNCFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


class TruncFileRespMsg : public SimpleIntMsg
{
   public:
      TruncFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_TruncFileResp, result)
      {
      }

      TruncFileRespMsg() : SimpleIntMsg(NETMSGTYPE_TruncFileResp)
      {
      }
};

#endif /*TRUNCFILERESPMSG_H_*/
