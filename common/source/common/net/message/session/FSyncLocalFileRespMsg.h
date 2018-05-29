#ifndef FSYNCLOCALFILERESPMSG_H_
#define FSYNCLOCALFILERESPMSG_H_

#include "../SimpleInt64Msg.h"

class FSyncLocalFileRespMsg : public SimpleInt64Msg
{
   public:
      FSyncLocalFileRespMsg(int64_t result) :
         SimpleInt64Msg(NETMSGTYPE_FSyncLocalFileResp, result)
      {
      }

      FSyncLocalFileRespMsg() :
         SimpleInt64Msg(NETMSGTYPE_FSyncLocalFileResp)
      {
      }
};


#endif /*FSYNCLOCALFILERESPMSG_H_*/
