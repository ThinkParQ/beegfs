#ifndef FLOCKAPPENDMSGEX_H_
#define FLOCKAPPENDMSGEX_H_

#include <common/net/message/session/locking/FLockAppendMsg.h>
#include <common/net/message/session/locking/FLockAppendRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <common/storage/StorageErrors.h>
#include <storage/FileInode.h>

class FLockAppendMsgEx : public MirroredMessage<FLockAppendMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<FLockAppendRespMsg, NETMSGTYPE_FLockAppend> ResponseState;
      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FileIDLock lock(EntryLockStore& store) override;
      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      FhgfsOpsErr clientResult;

      void forwardToSecondary(ResponseContext& ctx) override;
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return static_cast<FLockAppendRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "FLockAppendMsgEx/forward"; }
};

#endif /* FLOCKAPPENDMSGEX_H_ */
