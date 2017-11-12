#ifndef MKLOCALDIRMSGEX_H_
#define MKLOCALDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MkLocalDirMsg.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>

class MkLocalDirMsgEx : public MirroredMessage<MkLocalDirMsg, HashDirLock>
{
   public:
      typedef ErrorCodeResponseState<MkLocalDirRespMsg, NETMSGTYPE_MkLocalDir> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      HashDirLock lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      ResponseContext* rctx;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<MkLocalDirRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "MkLocalDirMsgEx/forward"; }
};

#endif /*MKLOCALDIRMSGEX_H_*/
