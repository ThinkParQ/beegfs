#ifndef UNLINKLOCALFILEINODEMSGEX_H_
#define UNLINKLOCALFILEINODEMSGEX_H_

#include <common/net/message/storage/creating/UnlinkLocalFileInodeMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileInodeRespMsg.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>

class UnlinkLocalFileInodeMsgEx : public MirroredMessage<UnlinkLocalFileInodeMsg,
   std::tuple<HashDirLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<UnlinkLocalFileInodeRespMsg,
         NETMSGTYPE_UnlinkLocalFileInodeResp> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      std::tuple<HashDirLock, FileIDLock> lock(EntryLockStore& store) override;

      bool isMirrored() override { return getDelEntryInfo()->getIsBuddyMirrored(); }

   private:
      ResponseContext* rctx;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<UnlinkLocalFileInodeRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override
      {
         return "UnlinkLocalFileInodeMsgEx/forward";
      }
};

#endif /* UNLINKLOCALFILEINODEMSGEX_H_ */
