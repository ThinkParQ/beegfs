#ifndef WRITELOCALFILERESPMSG_H_
#define WRITELOCALFILERESPMSG_H_

#include <common/net/message/SimpleInt64Msg.h>

class WriteLocalFileRespMsg : public SimpleInt64Msg
{
   public:
      WriteLocalFileRespMsg(int64_t result) :
         SimpleInt64Msg(NETMSGTYPE_WriteLocalFileResp, result)
      {
      }

      WriteLocalFileRespMsg() :
         SimpleInt64Msg(NETMSGTYPE_WriteLocalFileResp)
      {
      }
};

#endif /*WRITELOCALFILERESPMSG_H_*/
