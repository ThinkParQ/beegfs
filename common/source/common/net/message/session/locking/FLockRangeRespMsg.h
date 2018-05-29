#ifndef FLOCKRANGERESPMSG_H_
#define FLOCKRANGERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>


class FLockRangeRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @result FhgfsOpsErr_WOULDBLOCK if the lock could not be immediately granted
       */
      FLockRangeRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_FLockRangeResp, result)
      {
      }

      FLockRangeRespMsg() : SimpleIntMsg(NETMSGTYPE_FLockRangeResp)
      {
      }

   private:

   public:
      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /* FLOCKRANGERESPMSG_H_ */
