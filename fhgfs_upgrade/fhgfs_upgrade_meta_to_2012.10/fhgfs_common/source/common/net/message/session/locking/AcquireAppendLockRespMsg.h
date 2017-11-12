#ifndef ACQUIREAPPENDLOCKRESPMSG_H_
#define ACQUIREAPPENDLOCKRESPMSG_H_

#include <common/net/message/SimpleInt64Msg.h>


class AcquireAppendLockRespMsg : public SimpleInt64Msg
{
   public:
      AcquireAppendLockRespMsg(int64_t result) :
         SimpleInt64Msg(NETMSGTYPE_AcquireAppendLockResp, result)
      {
      }

      AcquireAppendLockRespMsg() :
         SimpleInt64Msg(NETMSGTYPE_AcquireAppendLockResp)
      {
      }
};

#endif /*ACQUIREAPPENDLOCKRESPMSG_H_*/
