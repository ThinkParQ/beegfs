#ifndef FLOCKAPPENDRESPMSG_H_
#define FLOCKAPPENDRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>


class FLockAppendRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @result FhgfsOpsErr_WOULDBLOCK if the lock could not be immediately granted
       */
      FLockAppendRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_FLockAppendResp, result)
      {
      }

      FLockAppendRespMsg() : SimpleIntMsg(NETMSGTYPE_FLockAppendResp)
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

#endif /* FLOCKAPPENDRESPMSG_H_ */
