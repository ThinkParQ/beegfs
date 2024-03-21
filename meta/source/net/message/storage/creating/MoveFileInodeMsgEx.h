#ifndef MOVEFILEINODEMSGEX_H_
#define MOVEFILEINODEMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MoveFileInodeMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>

class MoveFileInodeMsgEx : public MirroredMessage<MoveFileInodeMsg,
   std::tuple<FileIDLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<MoveFileInodeRespMsg, NETMSGTYPE_MoveFileInodeResp> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      std::tuple<FileIDLock, ParentNameLock, FileIDLock> lock(EntryLockStore& store) override;
      bool isMirrored() override { return getFromFileEntryInfo()->getIsBuddyMirrored(); }

   private:
      ResponseContext* rctx;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<MoveFileInodeRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "MoveFileInodeMsgEx/forward"; }
};

#endif /* MOVEFILEINODEMSGEX_H_ */
