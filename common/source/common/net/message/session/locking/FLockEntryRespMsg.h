#ifndef FLOCKENTRYRESPMSG_H_
#define FLOCKENTRYRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>


class FLockEntryRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @result FhgfsOpsErr_WOULDBLOCK if the lock could not be immediately granted
       */
      FLockEntryRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_FLockEntryResp, result)
      {
      }

      FLockEntryRespMsg() : SimpleIntMsg(NETMSGTYPE_FLockEntryResp)
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

#endif /* FLOCKENTRYRESPMSG_H_ */
