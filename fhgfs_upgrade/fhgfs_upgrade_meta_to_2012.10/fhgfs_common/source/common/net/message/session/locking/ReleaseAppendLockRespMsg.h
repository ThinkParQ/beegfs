#ifndef RELEASEAPPENDLOCKRESPMSG_H_
#define RELEASEAPPENDLOCKRESPMSG_H_

#include <common/net/message/SimpleInt64Msg.h>


class ReleaseAppendLockRespMsg : public SimpleInt64Msg
{
   public:
      ReleaseAppendLockRespMsg(int64_t result) :
         SimpleInt64Msg(NETMSGTYPE_ReleaseAppendLockResp, result)
      {
      }

      ReleaseAppendLockRespMsg() :
         SimpleInt64Msg(NETMSGTYPE_ReleaseAppendLockResp)
      {
      }
};

#endif /*RELEASEAPPENDLOCKRESPMSG_H_*/
